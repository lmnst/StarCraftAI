# Local Coding Agent workflow trial

You control a Protoss C++ BWAPI bot. Improve its play against a fixed local opponent.
This is a local workflow reproduction, NOT an official StarSkirmish submission.

Only edit the files in this trial's `src/starterbot/` directory. Keep the main event
loop, BWAPI connection, LocalTrace instrumentation and automatic-input restriction.
Do not read opponent source or change the opponent, game, map, tools or telemetry.
Do not use web access, external bot code or other agents during the timed trial.

Your tools are local source/API reads, source edits, and these commands from the
project root (replace TRIAL with the prepared trial name):

    & ./tools/Benchmark.ps1 -Action Build -Trial TRIAL
    & ./tools/Benchmark.ps1 -Action Practice -Trial TRIAL
    & ./tools/Benchmark.ps1 -Action Finish -Trial TRIAL

Build may need the local compiler parameters supplied by the operator. The manifest
fixes your time budget, practice seed, initial source and runtime hashes. Start is
called by the operator after setup and baseline measurement. Read the returned
feedback and version's feedback JSON before making another change. Each Build
saves a source snapshot, diff, build log and binary. Each Practice saves its result,
unit statistics and replay links. Failed/incomplete games are errors, not losses.

When time is nearly over, leave a compiling version and use Finish. Explain what
you changed, which feedback motivated the change and what remains uncertain.
Practice outcomes are local training data, not held-out evaluation or official scores.
