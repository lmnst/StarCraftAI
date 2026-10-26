# 星际争霸 C++ 程序本地演示

运行入口：E:\StarCraftAI\tools\LocalDemoLauncher.exe

- Run StarterBot：启动教学程序进行经济发展演示。
- Run UAlbertaBot：启动完整比赛程序。
- Stop demo：停止该启动器创建的游戏与程序；不会终止其他游戏目录的进程。
- 关闭启动器也会停止它创建的演示进程。

环境：Windows，StarCraft 1.16.1，BWAPI 4.4.0，MSVC v143，Windows SDK 10.0.22621.0。
地图：Heartbreak Ridge，神族对神族。
当前每次启动的是单程序对游戏内置电脑的独立运行测试。

## 文件位置

- 游戏：E:\StarCraftAI\StarCraft1161
- StarterBot 源码：E:\StarCraftAI\src\STARTcraft\src\starterbot
- UAlbertaBot 源码：E:\StarCraftAI\src\UAlbertaBot\UAlbertaBot\Source
- 重新编译两个程序：E:\StarCraftAI\tools\BuildBots.cmd
- 演示启动器源码：E:\StarCraftAI\tools\LocalDemoLauncher.cpp
- 状态采样源码：E:\StarCraftAI\tools\Telemetry.h
- 原始日志和状态 CSV：E:\StarCraftAI\logs

## 观察数字

CSV 列：frame,minerals,gas,supply_used,supply_total,workers,combat_units,buildings。
每隔24个游戏帧采样一次；单位和建筑数量包括程序已拥有的在建单位。
人口数字已将 BWAPI 的内部两倍人口值换算成界面单位。
资源变化、工人数量增长、人口上限增加，以及战斗单位出现用于核验程序确实在控制游戏。

## 自动验证

自动验证期间关闭了程序接受人工单位指令的选项。
StarterBot 在 StarterBot.cpp 中禁用了 Flag::UserInput。
UAlbertaBot 在 bin\UAlbertaBot_Config.txt 中设定 BWAPI.UserInput=false。
纯自动验证记录会分别保存为 *-automatic-validation.csv 和 *-validation-result.json。
先前含人工操作的调试记录已单独保留。

## 启动提示

进入游戏后若出现 StarCraft Tips，点击 OK 即可开始。
不要在该演示中登录 Battle.net；本地运行无需账号。
运行中不要再次启动 Chaoslauncher，以免多份游戏混在一起。
如需重新编译，先使用 Stop demo 停止相应程序。
