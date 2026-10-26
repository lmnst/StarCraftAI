# StarterBot 对战 UAlbertaBot：本地使用说明

本地准备、两个程序独立启动、同局对战、状态和事件记录、保存胜负和录像，已经跑通。

## 如何再次开始

双击 `E:\StarCraftAI\tools\MatchLauncher.exe`。

- `Start match (normal)`：正常观看速度，42 毫秒/帧。
- `Start match (fast)`：加速验证，10 毫秒/帧。实际速度受机器性能影响。
- `Stop match`：停止此启动器创建的游戏和机器人进程。提前停止不算完成比赛。
- `Replay last completed match`：正常速度打开最新一局的 StarterBot 视角录像。录像中可以移动镜头和查看单位，不会改变已经发生的比赛。

启动一局会停止此启动器上一次创建的对局或回放，并创建新的时间戳目录。关闭启动器也会停止它所属的进程。每次运行只打开一个对战启动器，避免重复房间名。无需战网账号。

两个真实游戏实例通过 BWAPI 的 `Local PC` 模式连接。主机玩家/房间名为 StarterBot；客机 UAlbertaBot 加入它。双方都是神族，在 `(2)Heartbreak Ridge.scx` 地图上进行 Melee 对局。每个外部 C++ 程序通过显式游戏 PID 绑定到自己的 BWAPI 服务。

## 哪里看数字

比赛中，启动器实时显示两方最新一条采样；各游戏画面也叠加本方数据。每 24 帧采样一次。

| 字段 | 含义 |
|---|---|
| frame | 游戏逻辑帧数，用它对应事件和比赛进程 |
| minerals / gas | 当前矿物 / 气体 |
| supply_used / supply_total | 已用人口 / 人口上限，已换算成游戏显示单位 |
| workers | 存在的工人单位数 |
| combat_units | 可攻击的非工人、非建筑单位数 |
| buildings | 存在的建筑数 |

单位和建筑统计包含在建/训练中的对象；`complete` 事件才能确认完成。24 帧约对应 1 个按 24 fps 换算的游戏秒，不等于加速运行时的现实秒数。录像保存的是游戏过程，不包含原机器人绘制的调试文字；原始 CSV 和事件文件保留了当时的数据。

## 每一局保存什么

最新目录记录在 `E:\StarCraftAI\matches\latest.txt`。每局独立保存在 `E:\StarCraftAI\matches\时间戳\`，不会覆盖上一局。

- `*-start.json`：本方/对手名称、玩家 ID、地图和地图哈希、多人模式。
- `*-binding.json`：各机器人的游戏、机器人、注入启动器 PID。
- `*-telemetry.csv`：连续状态采样。
- `*-events.csv`：单位创建、完成、形态变化、销毁和对局结束事件；列含帧数、单位 ID、所属玩家、类型和坐标。
- `*-result.json`：由 BWAPI 的 MatchEnd 回调记录胜负和终局帧数。
- `*.rep`：游戏原生录像；BWAPI 可能给第二实例追加 `[2]` 后缀。
- `*-process.log`、`*-game-launch.log`：程序和启动诊断。
- `validation.json`：验证脚本核验双方信息、终局回调、采样、事件和录像后的结果。

事件只包含该方 BWAPI 能观察到的单位，不能视为全地图所有事件。双方终局回调可能相差少量逻辑帧，本轮为 31 帧；验证允许不超过 120 帧，并要求双方胜负互补。

## 从画面找到代码

- StarterBot 策略：`E:\StarCraftAI\src\STARTcraft\src\starterbot\StarterBot.cpp`。`onFrame()` 调用采矿、补工人、建人口建筑函数；原始程序不造军队。
- UAlbertaBot 入口：`E:\StarCraftAI\src\UAlbertaBot\UAlbertaBot\Source\UAlbertaBotModule.cpp`，`onFrame()` 调用 GameCommander 更新。
- UAlbertaBot 策略配置：`E:\StarCraftAI\src\UAlbertaBot\UAlbertaBot\bin\UAlbertaBot_Config.txt`，当前神族策略为 `Protoss_ZealotRush`。
- 共同采样与事件代码：`E:\StarCraftAI\tools\Telemetry.h`，`LocalTrace::record/event/end`。
- 双进程联机和录像入口：`E:\StarCraftAI\tools\MatchLauncher.cpp`。
- 指定游戏 PID 连接：`E:\StarCraftAI\src\STARTcraft\src\bwapi\BWAPIClient\Client.cpp` 中的 `STARBENCH_SERVER_PID`。

本次增加的是观测、连接和保存逻辑，没有给任一方添加采矿/建造/战斗指令或投降条件。原有人工单位控制保持关闭；没有启用完整地图信息供机器人作弊。

## 构建和核验

`E:\StarCraftAI\tools\BuildBots.cmd` 重新编译机器人和对战启动器。

如需手动核验最新完整比赛，可在 PowerShell 运行：

```powershell
& 'E:\StarCraftAI\tools\Validate-Match.ps1'
```

验证脚本只适用于已经自然结束的比赛；提前停止的目录缺少终局文件时不会判为通过。

BWAPI 联机/配置依据：[官方配置文档](https://github.com/bwapi/bwapi/wiki/Configuration)。本机游戏和现有单人演示配置保留；未改变系统安全设置。