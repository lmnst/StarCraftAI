# StarCraftAI

在 Windows 上观察 C++ 程序通过 BWAPI 自动控制《星际争霸：母巢之战》：先运行单个机器人，再让 StarterBot 与 UAlbertaBot 在同一局对战，并保存数字、事件、胜负和原生录像。

## 已实现

- StarCraft 1.16.1 + BWAPI 4.4.0 的本机 Local PC 双实例联机。
- 两个外部 C++ 程序按游戏进程 PID 分别连接，人工单位控制关闭。
- 单机器人演示、双机器人比赛、普通/加速运行和录像回放入口。
- 每 24 帧记录矿物、气体、人口、工人、战斗单位和建筑数量。
- 记录单位创建/完成/形态变化/销毁事件，以及 MatchEnd 胜负回调。
- 每局独立保存 CSV、JSON、配置和 `.rep` 录像，并提供验证脚本。

## 仓库内容

```text
tools/       启动器 C++ 源码、工程、采样代码、构建和验证脚本
patches/     固定上游版本和恢复本地改动的补丁
docs/        使用说明、源码对应关系和实际验证记录
licenses/    第三方许可文本
```

游戏、编译器、第三方完整源码、已编译程序、个人运行日志和录像都不进入 Git。现有本地文件不受此规则影响。

## 环境准备

推荐项目目录 `E:\StarCraftAI`（已验证的本机位置）。需要 Windows、Git、兼容的 32 位 C++ 编译工具、Windows SDK 10.0.22621.0、合法取得的 StarCraft 1.16.1 和 BWAPI 4.4.0。

1. 克隆本仓库。
2. 在 PowerShell 运行 `& .\tools\Setup-Sources.ps1`，获取固定版本的 STARTcraft、UAlbertaBot 并应用补丁；已有符合版本的源码会检测补丁是否已应用。
3. 将游戏放在 `StarCraft1161`；将 BWAPI 4.4.0 发行包中 `Starcraft` 的内容合并到该目录。双实例启动需要 `StarCraft1161\injectory.x86.exe`，来源见 [第三方说明](THIRD_PARTY.md)。
4. 已验证的编译器位于 `tools\VSBuildTools`。若编译器安装在其他位置，调整 `tools\BuildBots.cmd` 中的 `MSBUILD` 路径。
5. 运行 `tools\BuildBots.cmd`，编译两个机器人和两个启动器。
6. 打开 `tools\MatchLauncher.exe`。正常/加速比赛结束后可点击 `Replay last completed match` 回放。

`src/`、`StarCraft1161/`、`matches/`、`logs/` 和本机编译器由准备/运行过程产生或由用户提供，均被 `.gitignore` 排除。单机器人演示若使用其他项目位置，需要相应调整 `tools/Telemetry.h` 的默认日志路径；双机器人比赛由启动器传递每局输出目录。

## 文档

- [双机器人对战与回放](docs/README-双机器人对战.md)
- [单机器人演示](docs/README-本地演示.md)
- [完整比赛验证记录](docs/双机器人验证记录.md)
- [单机器人自动验证](docs/自动验证记录.md)
- [上游来源与许可](THIRD_PARTY.md)

StarterBot 原始教学策略只采矿、补工人和建人口建筑；UAlbertaBot 使用神族 ZealotRush。记录中的胜负用于验证连接和观察流程，不是多对手性能榜单。