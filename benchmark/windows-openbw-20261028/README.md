# Windows OpenBW 工作记录（2026-10-28）

本文是整理 Git 时的阶段快照，后续比赛结果以各试验目录的原始记录为准。

## 已完成

- 本地接通十个对手、三张地图，冻结代码后按 10 × 3 × 6 跑 180 局。
- 击杀与建筑摧毁计分、86,400 帧封顶裁决已验证，替代早期 OpenBW 的零分占位。
- 开始计时前检查真实时间戳、编译、短局和双方回放；首轮与续跑核对实际模型、
  推理设置、工作目录写权限及网络限制。预检通过后才开始开发计时。
- Stardust 原二进制的独立 18 局检查出现 4 次异常；单独构建的源码修复候选
  三图各 6 局全部自然结束，36 份回放读取通过。当前名单使用该候选，
  不声称原发布 DLL 已修复，也不声称其版本与官方完全相同。

## 模型试验

`night56-20261027-2300` 保留为失败试验：工具故障占用前约 31 分钟，
模型将初始采矿骨架误当成策略限制；180 个任务得到 178 负、2 未决。
未决局未被丢弃，最终胜率为空。这不是完整一小时有效开发成绩。

修复入口后，`restart56-20261028-c` 使用 gpt-5.6-sol / medium，
北京时间 2026-10-28 10:57 至 11:57 开发一小时，随后冻结代码进入评测。
八批练习共 30 局、2 胜，胜场均来自 tscmoo 作者源码候选；练习过程中代码
和对手在变化，不能作为最终版本的固定评测胜率。本文整理时尚无最终 180 局摘要。

## 当前范围

名单为 Stardust-SourceRepairCandidate、PurpleWave、BananaBrain、Locutus、
tscmoo-AuthorCandidate、Steamhammer、McRave、HladHammer、PylonPuller、Skynet。
tscmoo 候选与官方 tscmoop2 的身份未确认，完整官方对手版本及三个 demo 未对齐。
这是本地十对手试验，不是官方 Benchmark 或榜单成绩。

旧原生环境的协议、冻结 Bot 和 65 场记录保持保留，未把新环境结果并入旧赛程。
出生位置不保证严格均衡。计分实现通过本地验证不等于证明官方环境完全一致。

## 本机证据位置

以下均相对 `D:/xjzb/openbw-readiness-20261027/`，运行产物不入 Git：

- `pipeline-acceptance/acceptance.json`：环境与工具验收。
- `score-validation.json`：计分与完整帧限验证。
- `repairs-20261028/REPORT.md`：修复交付与版本边界。
- `repairs-20261028/stardust-source-regression/matches/`：18 局回归。
- `trials/night56-20261027-2300/REPORT.md`：失败试验。
- `trials/restart56-20261028-c/manifest.json`、`notes.md`、`practice/`、`frozen/`：新试验。

运行依赖与入口见 [工具说明](../../tools/openbw-windows/README.md)。
