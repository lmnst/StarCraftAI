# StarCraftAI

Coding Agent 流程、各阶段工具和与官方 Benchmark 的差异见 [BENCHMARK.md](BENCHMARK.md)。
当前实验使用 [Windows OpenBW 工具](tools/openbw-windows/README.md)，本地十对手环境已通过验收；实验脚本仍依赖本机目录，不是完整可移植运行包。

让 C++ 机器人通过 BWAPI 自动控制《星际争霸：母巢之战》，观察比赛画面和运行数据。

## 现有内容

- StarterBot 与 UAlbertaBot 的单机演示和双机器人对战。
- 普通/加速比赛，以及比赛录像回放。
- 实时状态显示，保存资源、人口、单位事件和胜负记录。

## 目录

```text
tools/       启动器、数据采样、构建和验证工具
patches/     上游源码版本及本项目的修改补丁
licenses/    第三方许可文本
THIRD_PARTY.md  第三方项目来源
```

## 本地运行还需要什么

- Windows、Git、Visual Studio 2022 的 C++ 桌面开发工具和 Windows SDK。
- StarCraft 1.16.1，以及 BWAPI 4.4.0 的游戏端文件。
- 将游戏放在项目的 `StarCraft1161/` 中，并准备 `bwapi-data/BWAPI.dll`、`injectory.x86.exe` 和地图 `maps/BroodWar/AIIDE/(2)Heartbreak Ridge.scx`。来源见 [第三方说明](THIRD_PARTY.md)。

在项目目录打开 PowerShell：

```powershell
& .\tools\Setup-Sources.ps1
.\tools\BuildBots.cmd
.\tools\MatchLauncher.exe
```

`Setup-Sources.ps1` 获取固定版本的机器人源码并应用补丁；构建脚本自动寻找编译工具。单机器人演示使用 `tools/LocalDemoLauncher.exe`。

比赛数据和录像写入项目下的 `matches/`，单机日志写入 `logs/`。完整对局可用 `tools/Validate-Match.ps1` 检查。

项目可放在不同磁盘和目录，建议使用英文目录。游戏、编译器、第三方源码、生成的程序和运行数据由本地准备或生成，不包含在仓库中。
