Your objective is to maximize match win rate against the configured opponents.
Develop a complete Protoss C++ StarCraft: Brood War bot, including an economy,
construction, army production and combat. Choose and improve your own strategy.
src/Bot.cpp is an intentionally minimal STARTING POINT, not a strategy constraint.
You may replace its mining-only behavior completely. There is NO requirement to
remain mining-only, avoid attacking, or preserve the starter's strategy.
Use the full wall-clock budget until the hard UTC deadline. Improve code, compile,
play practice games, inspect feedback, and repeat. This is a LOCAL Windows OpenBW
experiment against ten locally configured versions, including a separately labeled
Stardust source-repair candidate and an explicitly unconfirmed tscmoo author-source
candidate; it is not an official StarSkirmish benchmark.

Only edit your trial's src/ and notes.md. You may read your trial directory, the
BWAPI API headers at D:/xjzb/openbw-probe-20261027-1140/source/bwapi/bwapi/include,
and the FAP/BWEM libraries at D:/xjzb/openbw-readiness-20261027/sdk. The tool compiles
every src/*.cpp using MSVC v141, Win32 Release, C++17, BWAPILIB, FAP headers and BWEM.
Keep the gameInit and newAIModule exports. Do not read opponent code, binaries or
configs, other trials, existing project bots, trusted engine code, or evaluation
seeds. Read isolation is cooperative. No internet, web, MCP or subagents. Do not
change tools, model, roster, engine, deadline, referee, or protocol.

Trial: TRIAL_PATH
Allowed operator tools, from PowerShell:
  & 'TOOLS_PATH/Practice.ps1' -Trial 'TRIAL_PATH' -Action Build
  & 'TOOLS_PATH/Practice.ps1' -Trial 'TRIAL_PATH' -Action Practice -Games 6
For targeted practice add -Opponents @('name1','name2') from your roster.json.
Practice allows 1..12 games, uses snapshots and fresh practice seeds, and returns
feedback and raw logs/replays. One practice batch at a time. Request automatic
approval review for this exact operator command when the sandbox blocks local
TCP or the compiler; do not request unrestricted shell/network access.

Use normal fog of war. No cheat flags, user input or map edits. Natural mutually
consistent engine results count. At frame 86400 the trusted referee compares
each player's own kill+razing scores from the local repaired scoring backend.
Ties are draws and earn zero. Infrastructure failures remain unresolved and are
never silently dropped from the denominator. Training is code development and
practice, not weight fine-tuning.

Check UTC time and use all remaining budget. If your turn ends early the operator
resumes the SAME session. At the deadline your process is stopped and src/ is
frozen. The operator builds collected source without edits, then evaluates ten
opponents x three maps x six fresh scheduled games = 180. Alternate launch order;
actual spawn coordinates are recorded, not claimed as guaranteed spawn balance.
The test cannot influence your training. Write short decisions to notes.md.
First record the win-rate objective and your initial playable strategy in notes.md.
Use practice wins/losses and game observations to improve that strategy. Idle-worker
counts, mineral stocks and survival time are diagnostic metrics, not the objective.
If repeated losses show that you have no buildings or fighting units, develop those
capabilities rather than accepting losses as a mining-only requirement.
If Build or Practice is blocked by infrastructure, report the exact failure; do not
claim successful training or reinterpret the task as a mining exercise.
