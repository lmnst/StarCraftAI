# Human Commander（人工高层决策按钮版）

在原版《星际争霸：母巢之战》1.16.1 中，用 F5–F9 五个按键给 bot 下达高层决策。底层的造兵、采矿、开矿、建造、目标选择和单位微操沿用 `v6-fix-r1-20261110` 策略，代码不改；人只决定主力姿态，以及什么时候额外开一片矿。

本目录是项目会议中"人做决策"一组的实现，用来和"代码做决策""LLM 做决策"两组对照，回答高层决策与底层执行分开是否必要。

## 1. 按键

| 按键 | 指令 | bot 的执行方式 |
| --- | --- | --- |
| F5 | 出击（attack） | 主力向 fix-r1 选出的目标推进：最近的已知敌方建筑；如果还没发现敌方建筑，就去敌方主基地或轮流巡视各矿点。单位级的集火、拉扯、残血后撤保持不变。 |
| F6 | 回防（defend） | 基地附近有威胁时就近迎敌；没有威胁时回到主基地。部队离目标超过 320 像素时只移动不交战，先赶回去。 |
| F7 | 集结（rally） | 到主基地前方的集结点整队，路上遇敌仍会交战。 |
| F8 | 撤退（retreat） | 只移动不交战，优先脱离战斗、回主基地。全队回到主基地 220 像素以内后，自动切换为集结。 |
| F9 | 开矿（expand） | 请求再开一片分矿，不改变当前姿态。 |

开局默认姿态是集结。姿态切换后一直有效，直到按下一个键。

F9 走的是 fix-r1 原有的建造队列，以下情况会被拒绝：
- 已经在等待开矿（`expansion_pending`）
- 已经有三片矿（`base_limit`）
- 晶体矿不足 400（`not_enough_minerals`）
- 没有合适矿点（`no_valid_site`）

请求被受理后，状态依次是：
- `queued`：已受理
- `planned`：已选好农民和矿点
- `started`：看到 Nexus 实际出现
- `completed`：Nexus 完工

受理后 600 帧内还没进入计划的，记为 `expired`；计划在开工前失败的，记为 `plan_failed`。fix-r1 自己的自动开矿仍然保留，与人工请求共用同一个建造计划，日志中用 `source: manual` 和 `source: auto` 区分。

游戏左上角会显示键位、当前姿态、最近一次 F9 的状态，以及是否有 Nexus 在建。镜头跟随己方主力。

## 2. 目录

```text
src/Bot.cpp            bot 源码：fix-r1 策略加指挥入口，约 900 行
src/ReplayAudit.cpp    回放播放用模块，只记录回放帧数，不下指令
CMakeLists.txt         构建脚本（依赖路径通过参数传入）
Candidate.def          DLL 导出表
Start-Demo.ps1         启动脚本：每次复制一份独立运行目录，然后启动原版游戏
game-source.example.txt  本机游戏目录示例
docs/baseline-v6-fix-r1-150-games.md  fix-r1 基线 150 局成绩
evidence/20261110-five-key/           五键实机验收的日志摘录（回放 .rep 按仓库规则不入库）
```

仓库里不包含游戏文件、BWAPI 发行版二进制、`ddraw.dll`，也不包含编译出来的 DLL。

## 3. 准备

- Windows，Visual Studio 2017（v141 工具集，Win32），CMake 3.12 或更高版本。
- 原版 StarCraft: Brood War 1.16.1 游戏目录，需要包含以下内容：
  - `StarCraft.exe`、各个 MPQ 文件
  - `injectory.x86.exe`
  - `bwapi-data/BWAPI.dll`（BWAPI 4.4.0）
  - 地图 `maps/BroodWar/AIIDE/(2)Heartbreak Ridge.scx`
- 原生 BWAPI 4.4.0 的头文件和 `BWAPI.lib`。注意必须是原生游戏用的版本，不能用 OpenBW 的 `BWAPILIB.lib`。可以使用 STARTcraft 自带的 `src/bwapi/include` 和 `visualstudio/Win32/Release/BWAPI/BWAPI.lib`。
- BWEM 源码（MIT 协议），需要包含 `BWEM/src` 和 `BWEM/include` 两个目录。
- cnc-ddraw 的 `ddraw.dll`（MIT 协议），放在本目录下。它负责让游戏以窗口模式、用 GDI 软件渲染运行。

## 4. 构建

```powershell
cmake -S . -B build -G "Visual Studio 15 2017" -A Win32 `
  -DBWAPI_INCLUDE="<STARTcraft>/src/bwapi/include" `
  -DBWAPI_LIB="<STARTcraft>/visualstudio/Win32/Release/BWAPI/BWAPI.lib" `
  -DBWEM_DIR="<BWEM 根目录>"
cmake --build build --config Release
```

构建产物是 `build/Release/Candidate.dll`（bot）和 `build/Release/ReplayAudit.dll`（回放审计）。

## 5. 对战

把 `game-source.example.txt` 复制为 `game-source.txt`，填写本机游戏目录（`game-source.txt` 不入库）；也可以每次给启动脚本传 `-GameSource`。然后运行：

```powershell
& ./Start-Demo.ps1
& ./Start-Demo.ps1 -Speed 42     # 放慢；参数会写入 bwapi.ini 的 speed_override，范围 8–80
```

- 地图固定为 Heartbreak Ridge。己方神族，对手是原版内置的神族电脑。
- 如果出现 `Starcraft Tips` 窗口，点 `OK` 关掉。
- 启动脚本每次都会新建 `runs/<时间>/` 作为独立运行目录，不会改动原游戏目录。
- 自然结束的对局，回放保存在 `runs/<时间>/maps/replays/command-demo-<时分秒>.rep`；游戏可能会在文件名后追加实例编号。
- 用 Quit Mission 中途退出也会生成回放。但游戏随后可能自动开新局，同一窗口里的下一局日志会带 `-2`、`-3` 等后缀。这个防覆盖的后缀轮转已经编译进去，但还没有在实机上复测过。

## 6. 日志

日志都写在 `runs/<时间>/bwapi-data/write/` 下，每行一个 JSON：

| 文件 | 内容 |
| --- | --- |
| `commander-state.jsonl` | `key_received`：收到按键的帧；`command`：指令生效的帧、结果状态和生效后的姿态；`state`：每 120 帧一次，含经济、部队、部队中心和实际目标、敌情、已知敌方建筑及最后看到的帧；`expansion_*`：开矿各阶段；`retreat_complete`；`end`：胜负 |
| `astra-trace.jsonl` | fix-r1 策略自身的跟踪：建造计划、工人防守、隐形单位、每 240 帧一份经济概况 |

指令一般在收到按键后 0–8 帧内生效，因为策略每 8 帧更新一次。回放中不显示屏幕上的提示文字，需要用 `commander-state.jsonl` 里的帧号去对照回放。

## 7. 播放回放

```powershell
& ./Start-Demo.ps1 -Replay <回放路径>.rep
```

五键验收回放 `command-demo.rep`（约 126KB）因仓库排除 `*.rep` 而未入库，需要时向作者索取后放到 `evidence/20261110-five-key/`。

播放时使用同一个原版游戏，加载 `ReplayAudit.dll`；审计记录写入 `runs/<时间>/bwapi-data/write/replay-audit.jsonl`。原版内置电脑参与的回放，现有的 OpenBW 回放入口会识别成 ComputerAI，无法播放，需要用这里的原版播放方式。

## 8. 验收记录（2026-11-10，Heartbreak Ridge，神族对原版神族电脑）

五键主验收局。所有动作完成后用 Quit Mission 结束，以便生成回放。

| 操作 | 收到帧 | 生效帧 | 游戏内结果 |
| --- | ---: | ---: | --- |
| F5 进攻 | 20 | 24 | 从集结切换为出击 |
| F9 人工开矿 | 8550 | 8552 | 8568 帧生成计划，8776 帧观察到 Nexus 开工，10647 帧完工（单位编号 244，位置 `(111,28)`），期间军队保持进攻 |
| F6 回防 | 11319 | 11320 | 前线部队转向基地 |
| F7 集结 | 12948 | 12952 | 军队目标切换为集结点 |
| F8 撤退 | 15064 | 15064 | 部队脱离前线，16104 帧回到主基地附近，自动切回集结 |
| F5 再次进攻 | 16603 | 16608 | 重新出击 |

另外两局专项测试：
- 第一局验证了集结时部队中心向集结点移动，以及矿不足、自动开矿在建、达到三矿上限三种情况下 F9 被拒绝。
- 第二局验证了人工开矿计划在开工前失败时，正确记录为 `expansion_plan_failed`。

有三种情况本轮没有触发，所以没有验证：无矿点、排队超时、开工后 Nexus 被毁。

两键版早期验收局获胜，终局回调在第 16897 帧，回放完整播放到第 16943 帧。

日志摘录放在 `evidence/20261110-five-key/`（回放文件未入库，见第 7 节）。五键回放已在原版游戏中播放到结算画面，回放头记录的总帧数是 25818。这局是主动退出的，所以没有审计结束帧。退出后自动开的新局覆盖了实时日志，因此 `command-excerpts.jsonl` 保存的是退出前已经核对过的字段摘录。

## 9. 与基线和其他两组的关系

- 策略部分与 `v6-fix-r1-20261110` 相同。fix-r1 在 150 局赛程中的有效胜率为 39.86%（57 胜 86 负，另有 7 局异常），详见 `docs/baseline-v6-fix-r1-150-games.md`。
- 环境变量 `STARCRAFT_COMMAND_MODE` 的取值决定模式：
  - `demo`：键盘模式，本说明介绍的就是这个模式
  - `external`：从 `bwapi-data/write/commander.commands` 读取同样五个指令，供脚本调用
  - 不设：原策略
- 人工这一组目前在原版游戏中对战原版电脑。代码组和 LLM 组在 OpenBW 上对战固定的 bot 对手（另见 LLM 指挥版本）。两种环境的胜率不能直接比较。人工组的对局环境还没有和另外两组统一。

## 10. 第三方组件

- BWAPI 4.4.0（LGPL-3.0）
- BWEM（MIT/X11）
- cnc-ddraw（MIT）
- STARTcraft（MIT），提供 BWAPI 头文件和库

以上都需要使用者自行获取，本目录不分发。
