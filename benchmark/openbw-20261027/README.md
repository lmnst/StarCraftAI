# OpenBW 快速练习工具验收

当前范围仅为练习工具。**没有启动新一小时独立试验，没有运行正式 234 局评测。**
旧 `runs/local234-20261026/` 的 65 场结果、冻结 Bot、回放和日志保持原样，旧评测仍暂停。

## 已验收

- 无界面 OpenBW；每局独立本地通信目录、工作目录和读写夹具，默认四路并行。
- 按次调用后退出的编译 / 练习命令，没有后台请求队列或定时任务。
- 每次编译保存源码快照、日志和二进制哈希；练习使用独立程序快照。
- 自然终局需双方结果互补，并保存非空回放；异常单独记录，不填成胜负。
- 摘要包含首次创建 / 完成帧、己方损失时段，以及资源、人口、闲置工人采样。
  损失时段只包含己方损失，不代表完整敌我交战信息；经济采样不等于因果诊断。
- 三张地图的自然终局回放、短帧限回放均通过无界面读取；真实 86400 帧终止也已观察到。

同一六场输入清单的实测（实际 Bot 逻辑存在随机性，不保证逐帧重现）：

| 并行数 | 批次用时 | 局 / 分钟 |
| --- | ---: | ---: |
| 1 | 54.349 秒 | 6.624 |
| 2 | 30.557 秒 | 11.781 |
| 4 | 24.119 秒 | 14.926 |

独立验证实际 PowerShell 练习入口：采矿骨架对四个练习夹具混合六场，全部自然结束，
批次用时 41.310 秒（不含此前编译），没有未解决运行异常。这些是工具验证，不是 Bot 实力成绩。

机器 / 对手 / 局势会影响速度；上述数字不能直接外推所有正式对手。
证据见 `runs/openbw-20261027/acceptance.json` 和 `tool-acceptance-final/`。

## 评分限制：不能把 0:0 当平局

固定的公开 OpenBW 移植在 `OpenBWData/BW/BWData.cpp` 中将
`Player::allKillScore()`、`Player::allRazingScore()` 写成 `return 0; // fixme`。
验收局已发生击杀，但原始 API 比分仍是 0:0。因此工具将帧限局标记为
`frame_cap_unscored`，胜 / 负 / 平均不增加，最终分数保持空值。

最初验收曾把这些帧限局当成平局，该结论已撤销。原始数据保留，每条受影响记录旁有
`scoring-correction.json`；修正后另跑三张地图回归，均返回“未计分”，没有伪造平局。
自然结束的互补胜负仍可用于练习反馈。**正式评分后端尚未就绪。**

## 对手与官方材料

当前只配置四个练习夹具：Locutus、Steamhammer 的公开源码版本，以及两个历史 Astra
参考版本。后两者仅用于工具验收，不属于正式人工对手名单的替代方案。

人工 Bot 并非全都未开源：部分有独立公开源码或 Windows 发布包。当前未接齐的是
官方实际使用的完整对手版本、配置、演示 Bot 和评分环境。Windows DLL 不能直接作为
Linux `.so` 加载；公开源码仍需移植并核对版本。

- [官方 Hillclimb 名单](https://starskirmish.com/hillclimb/)
- [公开历史 LLM Bot 与 OpenBW 示例](https://github.com/llmskirmish/starskirmish-hillclimb)
- [已使用的 Locutus / Steamhammer 移植源码](https://github.com/bmnielsen/StardustDevEnvironment)

曾考虑的“2 人工 + 8 历史 LLM + 3 本地演示”正式名单方案已撤回；没有用它评测新 Agent。
后续正式评测应优先按官方人工对手名单补齐，缺失项逐一注明，不默认换成 LLM 对手。

公开 OpenBW 的 BWAPI 标识为 4.2.0，官方概述写 4.4.0；当前环境据公开示例搭建，
不能称为完全相同的官方环境。`candidate_host` 字段目前仅表示预期启动顺序，
没有验证实际主机角色，也不保证出生位置均衡；实际出生坐标保存在裁判日志中。

## 调用

在候选目录放置 `src/Bot.cpp`（或多个 `.cpp` / `.h`），导出 `gameInit` 和 `newAIModule`。
使用仓库中的 `tools/openbw/Practice.ps1`：

```powershell
& .\tools\openbw\Practice.ps1 -Trial '候选目录绝对路径' -Action Build
& .\tools\openbw\Practice.ps1 -Trial '候选目录绝对路径' -Action Practice -Games 6
```

指定 `-Opponents @('Locutus-PublicSource','Steamhammer-PublicSource')` 可定向练习；
默认在四个夹具、三张地图和两种启动顺序间轮换。一次最多 12 局，请勿同时对同一候选
目录启动多个练习命令。每次输出简短摘要，完整记录在候选的 `builds/`、`practice/`。

`RunAgent.ps1` 仍是未验收的草稿，且因完整名单和评分未就绪而拒绝启动。
当前用户指令是仅验收工具；任何独立试验继续保持未启动。
