# 第三方来源

| 项目 | 用途 | 来源与许可 |
|---|---|---|
| STARTcraft / StarterBot | 基础机器人 | [上游项目](https://github.com/davechurchill/STARTcraft)，[MIT 许可](licenses/STARTcraft-MIT.txt) |
| UAlbertaBot | 对战机器人 | [上游项目](https://github.com/davechurchill/ualbertabot)，[项目许可](licenses/UAlbertaBot-MIT.txt)及其组件声明 |
| BWAPI 4.4.0 | 游戏接口 | [发行版本](https://github.com/bwapi/bwapi/releases/tag/v4.4.0)，[LGPL v3 许可](licenses/BWAPI-LGPL-3.0.txt) |
| StarCraft AI Tournament Manager | 提供游戏注入启动工具 | [上游项目](https://github.com/davechurchill/StarcraftAITournamentManager)，`injectory.x86.exe` 位于 `server/required/Required_BWAPI_440.zip` |

机器人源码版本记录在 `patches/sources.lock.json`，本项目的连接、观测和构建修改保存在补丁中。完整源码由准备脚本获取，遵循各上游项目及组件的许可。

StarCraft 1.16.1 游戏和比赛地图需自行准备；仓库不分发游戏、注入器、BWAPI 发行二进制或微软编译器。