You are developing a Protoss C++ StarCraft: Brood War bot from an only-mining skeleton.
You have one full hour of wall time, ending at the manifest UTC deadline. Work independently
to maximize wins against the 13-entry local OpenBW roster listed in roster.json.
This is explicitly a LOCAL experiment, not an official StarSkirmish benchmark score.

Only edit files within this trial's src/ and notes.md. Start with src/Bot.cpp, which already
exports gameInit and newAIModule and implements only idle-worker mineral gathering.
You may add C++ source/header files to src/. The compiler builds every src/*.cpp, using
C++20, -O2, BWAPI, FAP and a precompiled BWEM library. Keep the source compiling.

Allowed read sources: your own trial folder; BWAPI headers at
REPO_PATH/../../upstream/bwapi/bwapi/include; FAP and BWEM at
REPO_PATH/../../upstream/starskirmish-hillclimb/libs; and the two operator tools below.
Do not read opponent implementations/binaries/configs, other runs, previous trial code,
top-level bot code, historic LLM submissions, held-out seeds or evaluation results.
No internet, Web, MCP services, subagents or other agents. Do not change trusted tools,
engine settings, the model, the deadline, roster, referee or protocol. Directory-read
isolation is cooperative; comply even when the OS would technically permit reading.

Run these PowerShell commands from your trial directory:
  & 'REPO_PATH/tools/openbw/Practice.ps1' -Trial $PWD -Action Build
  & 'REPO_PATH/tools/openbw/Practice.ps1' -Trial $PWD -Action Practice -Games 6
For a targeted batch, add -Opponents @('name1','name2') using names from roster.json.
Each invocation is a fixed one-shot WSL build or parallel batch and exits when finished.
It requires approval review; you may request escalation only for that exact operator
command. Do not use other WSL or network commands. Practice supports 1..12 games;
default batches rotate through the roster across three maps and both host roles.
Do not run multiple practice commands simultaneously. You get a concise result summary,
first completion timings, own losses and sampled supply/economy observations. Raw
JSONL traces and replays remain in your practice folder for deeper inspection.
Each build uses a source and binary snapshot, so you can safely edit after it starts.

Check actual UTC time and use the whole budget for implementation and practice.
Write short decisions in notes.md. Do not finish after one successful build. If you
finish a turn early, the operator resumes the SAME session until the hard deadline.
At the deadline, your process is stopped and src/ is frozen. Collected source is
compiled without changes; held-out evaluation cannot influence your implementation.

Rules: no full-map or user-input flags; normal fog of war. Natural engine wins count.
At frame 86400 the referee compares each player's OWN kill+razing score; ties draw.
Unresolved infrastructure errors are recorded separately and never silently omitted.
Final local evaluation is 13 opponents x 3 maps x 6 preregistered fresh seeds = 234.
Draws earn zero in wins/234*100. This is one independent run, not a model average.

Public environment differences: OpenBW's public BWAPI port identifies as 4.2.0,
where the official overview says 4.4.0. The local roster contains public versions
and explicit substitutes because exact official versions and demo sources were not
obtained. Do not describe a resulting score as official or directly comparable.
