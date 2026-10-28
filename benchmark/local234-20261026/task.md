You are writing a Protoss C++ StarCraft: Brood War bot against BWAPI 4.4.0.
Your objective is to maximize wins against a fixed diverse local roster. You have
one hour of wall time, counted from the manifest start through the deadline.
Work independently. This is a local reproduction, not an official benchmark run.

Only change src/Bot.cpp in this trial. It initially only sends idle workers to
minerals. Implement Bot::onStart and Bot::onFrame (declared in tools/local234/Bot.h).
You can add helper functions and standard headers in that file. The trusted main
loop, telemetry, game connection, frame cap and automatic input restrictions are
outside the editable file. Never change them or any build/practice/runtime files.

Read local BWAPI headers at REPO/src/STARTcraft/src/bwapi/include. You may read the
FAP/BWEM libraries at REPO/../../upstream/starskirmish-hillclimb/libs. No external
bot implementation may be copied or read. Do not read opponent source, binaries,
runtime files, other runs, hidden evaluation seeds or evaluation results. Do not
use the internet, web tools, MCP services, subagents or additional agents.

From this trial directory use exactly these operator tools (REPO is the path in
manifest.json):
  & REPO/tools/local234/Build.ps1 -Trial $PWD
  & REPO/tools/local234/Practice.ps1 -Trial $PWD -Opponent UAlbertaBot
Practice also supports Stardust, BananaBrain, Locutus, HladHammer, Steamhammer,
Odin, EggBot, Venator, Orion, LocalZealotRush, LocalFourGateDragoon, LocalDTRush. All games are PvP.
Practice is only on Heartbreak Ridge, with fixed seed 20261026. Use feedback and
your own telemetry to improve, record your decisions in notes.md, and keep the
source compiling. Each build saves source, binary, hashes and compiler logs.
You can use the entire budget for iterations. Read manifest.json for the deadline
and check actual UTC time. Do not stop after a first successful build or game.
Leave a compiling version before time expires. At the deadline the operator stops
your process, copies all final source and binaries and compiles the frozen source
without modification. No hidden test feedback will be returned during training.

Rules: enemy elimination wins; at frame 86400 the higher BWAPI kills + razings
score wins, ties are draws. Draws score zero under wins / 234 * 100. Crashes are
adjudicated separately from infrastructure errors. Full final testing is 13
distinct opponents on 3 two-player maps, six games per combination, fresh seeds.
Environment differs from official: native StarCraft 1.16.1, local selected bot
versions and local roster substitutions. Folder/network isolation is partly
cooperative; follow the restrictions even when OS access would allow more.
