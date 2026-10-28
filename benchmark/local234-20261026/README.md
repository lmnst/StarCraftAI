# 234 局本地复现工作记录

归档说明（2026-10-28）：下文保留准备阶段记录。之后该原生环境已运行 65 场，
目前暂停，尚无完整 234 局成绩。后续工作改用独立的 Windows OpenBW 十对手环境，
见 [当前记录](../windows-openbw-20261028/README.md)。本目录工具依赖原有本机
`runs/local234-20261026/`，不应直接重建协议或启动旧试验。

当前尚无最终成绩。此前 verified-loop 的 v002 不作为本轮参赛 Bot。

官方规则证据来自 https://starskirmish.com/bench/v0-2/ 及其公开 data.js。
官网没有发布完整起始包、三个 demo 实现、13 个对手的精确版本或六局配对代码。
本轮只能称本地复现：原生 StarCraft 1.16.1 / BWAPI 4.4.0，非 OpenBW。

候选阵容是 Stardust、BananaBrain、Locutus、HladHammer、Steamhammer、
UAlbertaBot、Odin、EggBot、Venator、Orion，以及本地编写的 LocalZealotRush、
LocalFourGateDragoon、LocalDTRush。前三个本地 demo 的源代码不冒充官方 demo。
五个同名人工 Bot 的精确官方版本也未确认。其余五个人工 Bot 是明确的替代项。
SSCAIT 公共元数据、原包和对应 BWAPI DLL 保存在本地 packages/；原始网页和
CLI 探针事件保存在 sources/。这些依赖不加入 Git。

实际比赛证据、版本和协议放在 `runs/local234-20261026/`。初始骨架只采矿，
与以前带工人生产和补人口的 STARTcraft 起点不同。旧 runs/、matches/ 保留。

新工具在 `tools/local234/`：

- Match.cpp：逐局独立 Windows Job、双方实例和 PID 绑定、超时、退出码、双回放。
- Referee.cpp：双方各自记录自己的 kills + razings，避免用迷雾下的敌方零分裁决。
  通过 BWAPI 官方 TournamentModule 禁止人工/全图信息、固定速度并关闭绘制。
- Client.cpp：固定事件循环、遥测、86,400 帧封顶；短局兼容检查通过独立环境变量
  设置 240 帧，不计入评测，正式调度会清除此变量。
- Build.ps1：每次 Rebuild，保存源码、程序、哈希和编译日志。
- RunAgent.ps1：独立 Codex、固定 gpt-6.1-sol / medium、一小时外部计时、JSONL 事件、
  到点停止并收集源码和程序，再原样编译收集到的源码。
- PrepareProtocol.ps1 / Evaluate.ps1：固定 234 个唯一条目和新种子、双方身份/地图
  哈希、断点续跑、最多两次基础设施重试、逐条证据、CSV/JSON 与分项统计。

默认约定：每对手/地图六个不同新种子、双方各主持三局；出生位置由引擎 RNG
产生并逐条记录，**不声称出生位置严格三比三**。自然结束须双方互补；到点比较
双方独立分数，同分平局。单方有证明的 Bot/游戏进程崩溃判负；双崩平局；
无法证明的失败留作未解决基础设施问题，不能从 234 分母中排除。完整规则以
计时前保存的 protocol.json 为准。分数只在 234 个条目均裁决后产生：胜局/234×100。

## 已发现的问题

独立 Codex 的登录和模型可用，但默认策略拒绝了 Get-Date 和文件补丁。单次
`--approve-for-me` 探针可正常运行；将这一模式用于一小时脚本的操作被自动审批
审查拒绝，理由是未经明确授权扩大自动批准范围。目前没有修改全局设置，也
没有在限时脚本中启用该模式；已向用户请求仅本次试验的许可。

Locutus 的 SSCAIT 包在开局后出现 0xC0000005，原尝试保留。其官方 README 说明
需要 BWTA v6 缓存；已从 Orion 公开包取得三图缓存。另按官方源代码提交
4e96da0b7e31831ee97aaef153b2bef977a235e1 重编译，只改编译配置，不改策略代码，
独立保存在 Locutus-rebuilt 运行目录。该重编译版已经在三张地图各通过一场短局验证，已纳入本地阵容。

## 复跑入口

在实际仓库根目录运行；真实游戏需正常桌面权限：

```powershell
# 协议与赛程已固定，不要重复 Prepare。
& ./tools/local234/RunAgent.ps1 -Name agent-001 -BudgetMinutes 60
# 使用已经冻结的那个 Bot；同一 OutputName 会跳过已有逐条结果，保留失败尝试。
& ./tools/local234/Evaluate.ps1 -Frozen ./runs/local234-20261026/agent-001/frozen -OutputName evaluation-001
```

RunAgent 已获用户明确授权并启动，Evaluate 已用只采矿基线完成一个真实条目，生成 CSV/JSON、分项统计及双回放；重复运行同一 OutputName/Limit 后复用原结果，尝试目录仍只有一个。完整 234 局尚未运行。

已保存 protocol.json、schedule.json、protocol-sha256.json。已核对 13 个对手、3 张地图、39 个组合，每组合六局、三局主持，234 个条目 ID 和种子均唯一。短局与基线对局不计入正式评测，当前正式完成数为 0/234。

调度器验证位于 runs/local234-20261026/harness-smoke/，1/234、0 胜 1 负，score 为 null。这是基础设施验证，不能代替独立 Agent 的成绩。
