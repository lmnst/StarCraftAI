# Coding Agent 写 Bot：实验入口

## 当前状态（2026-10-28）

当前使用 [Windows OpenBW 工具](tools/openbw-windows/README.md)：本地十个对手、
三张地图，每组合六局，共 180 局。环境及计分已通过本地验收；Stardust 使用
明确标记的源码修复候选，tscmoo 作者源码候选与官方 tscmoop2 的身份尚未确认。
这些结果不能称为官方 Benchmark 成绩。

最新阶段记录见 [Windows OpenBW 工作记录](benchmark/windows-openbw-20261028/README.md)。
对战后端已收进仓库，机器路径统一由不入库的 `local-paths.json` 配置。
引擎、对手和已验收的运行环境仍需另行准备，见工具说明。

| 阶段 | 入口 | 状态 |
| --- | --- | --- |
| 首次代码修改闭环 | `tools/Benchmark.ps1` | 2026-10-26 已验证，下文为当时记录 |
| 原生游戏 234 局 | [工作记录](benchmark/local234-20261026/README.md)、`tools/local234/` | 已有 65 场记录，暂停，非最终成绩 |
| WSL OpenBW 练习 | [验收记录](benchmark/openbw-20261027/README.md)、`tools/openbw/` | 早期练习工具，封顶局未计分 |
| Windows OpenBW | [工具说明](tools/openbw-windows/README.md)、`tools/openbw-windows/` | 当前本地试验入口，计分已验证 |

游戏、第三方完整源码、编译器、对手二进制、`runs/` 和 `matches/` 均留在本机。
后续运行前应核对对应环境记录，不要混用不同阶段的工具、名单或成绩。

## 2026-10-26 首次流程验证记录

目标是让 Agent 修改代码、对战、读反馈、再修改。当前基于同事项目，分支
`benchmark-agent-loop`。原有 GUI 继续使用；试验代码放在独立 `runs/`，
不覆盖同事的 StarterBot 或对手。

[首次流程验证结果](benchmark/workflow-smoke-20261026.md)：已完成两次反馈驱动的修改与对战，保存代码、结果和回放；不算官方成绩。

## 官方框架公开到什么程度

截至 2026-10-26 的检查结果：

| 部分 | 已确认 | 还缺什么 |
| --- | --- | --- |
| 规则 | [v0.2 官方说明](https://starskirmish.com/bench/v0-2/)公开 | 原始提示词、起始文件包、完整工具实现未找到 |
| Agent | 官方使用各厂商 CLI，包含 Codex | 本机 CLI 版本不同；独立试验还需固定模型和推理设置 |
| Bot 参考 | [hillclimb 仓库](https://github.com/llmskirmish/starskirmish-hillclimb)有历史 Bot、FAP/BWEM 和运行脚本 | 这个仓库不是完整 Benchmark 框架 |
| 评测 | 固定对手、多地图、新种子 | 完整对手版本、种子、原始摘要生成器尚未取得 |
| 正式打榜 | 官网有结果与规则 | **自行提交入口未确认**，本地胜率不能叫官方成绩 |

官方 v0.2 给 Agent 一小时，神族 C++ / BWAPI 4.4.0，在 OpenBW 上运行。
每个最终 Bot 对 13 个对手，在三张地图上各打六局（234 局），每模型有五次
独立运行；评分取胜率。练习可编译、对战并读取摘要/记录，禁止互联网。
是否另外开放完整原始框架，仍需向作者或负责人确认。

## 首次流程验证的边界

当时为原生 StarCraft 1.16.1 / BWAPI 4.4.0，单张 Heartbreak Ridge 地图、
一个 UAlbertaBot 对手、固定练习种子。起始代码是同事准备的 STARTcraft，
已带工人生产和补人口，**并非官方只挖矿的起始 Bot**。

这次只验证流程。当前会话看过项目信息，不能算独立盲测；目录/时间限制是
协作协议，不是操作系统级隔离。尚无官方完整文字复盘、隐藏测试集和计时收集器。

## 准备和调用

按 README 准备游戏、依赖并构建 BWAPI 和对手，然后运行：

```powershell
& $msbuild ./tools/BenchmarkMatch.vcxproj /p:Configuration=Release /p:Platform=Win32
& ./tools/Benchmark.ps1 -Action Prepare -Trial smoke-001 -BudgetMinutes 10
& ./tools/Benchmark.ps1 -Action Build -Trial smoke-001
& ./tools/Benchmark.ps1 -Action Practice -Trial smoke-001
& ./tools/Benchmark.ps1 -Action Start -Trial smoke-001
# Agent 修改 runs/smoke-001/src/starterbot，再 Build、Practice、读反馈并修改。
& ./tools/Benchmark.ps1 -Action Finish -Trial smoke-001
```

Build 用 `-MSBuild` 或 `MSBUILD_EXE_PATH` 指定编译器。本机仅有 VS 2017，
明确传入 `-Toolset v141 -WindowsSDK 10.0.17763.0`；默认仍为同事的 v143。
首次构建 BenchmarkMatch 时，同样传 `/p:PlatformToolset=v141`
和 `/p:WindowsTargetPlatformVersion=10.0.17763.0`。

GUI 与命令行共用对战逻辑，游戏仍有窗口，不是 Headless。旧游戏在本机沙箱中
启动失败，实际对战需要正常桌面环境。

试验会保存：初始源码及哈希、任务、时间、环境哈希，每版代码/差异/编译记录/
程序，每场反馈和比赛目录。回放和原始数据在 `matches/`，失败或超时不算胜负。
Finish 冻结最后一版预算内构建成功的程序，允许提前结束；这不同于官方到点
收集整个 Bot 目录。

## 后续独立 Codex 试验

本机 CLI 版本和登录状态已检查。依据 [Codex 官方文档](https://developers.openai.com/codex/noninteractive/)，
可用 `codex exec --json --sandbox workspace-write -C <trial-directory> -` 从标准
输入读固定任务并保存事件日志。独立运行前还需限制工具和网络、固定模型/推理
设置，并由外层计时器停止 Agent、冻结文件，再做隐藏种子评测。
当前侧边会话没有启动其他 Agent，也没有执行正式独立 CLI 试验。
