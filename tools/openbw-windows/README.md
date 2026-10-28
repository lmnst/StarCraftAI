# Windows OpenBW local trial

Current local environment (2026-10-28): ten opponents, three maps, six games per
pair, 180 evaluation games. Scoring and operator preflight have passed local
acceptance. Stardust uses an explicitly labeled source-repair candidate that
passed 18 three-map regression games; the retained release DLL is not repaired.
See the [dated progress record](../../benchmark/windows-openbw-20261028/README.md).

## Local prerequisites

These scripts archive the current machine's experiment. A fresh clone cannot
run them on its own. They currently require these paths and external files:

- `D:/xjzb/openbw-readiness-20261027/`: accepted `environment.json`, acceptance
  records, opponent runtimes, maps, engines and FAP/BWEM SDK files.
- `D:/xjzb/openbw-probe-20261027-1140/`: BWAPI headers and `BWAPILIB.lib`.
- `E:/anaconda_c/python.exe` and the VS 2017 compiler/CMake under `E:/VS1019/`.
- The enclosing workspace's `build/windows-openbw-integration-20261027/match_backend.py`,
  loaded by `practice.py`; that backend also requires the enclosing workspace's
  `upstream/bwapi/bwapi.ini.example` and existing local fixture directories.
- A locally configured Codex CLI for timed trials.

Runtime configuration, third-party source trees, binary dependencies, model
session logs and game replays are not distributed in this repository. Preserve
the external workspace alongside this source archive when backing up experiments.

## Development and evaluation

The objective is match win rate. `Starter.cpp` is a replaceable minimal starting
point; its mining behavior does not restrict construction, army production or
combat. `task.md` supplies the full training instructions. Resume prompts repeat
the objective instead of relying only on the agent's saved notes.

Before starting a model, `StartTrial.ps1` runs `Preflight.ps1`. This creates an
independent directory under `D:/xjzb/openbw-readiness-20261027/preflight/`, writes a
manifest with the same seven-digit .NET deadline used by timed trials, invokes
the public Practice wrapper's Preflight action (same build and match code), and
requires one resolved 1,200-frame game with readable replays. This operator check
uses a 120-second match timeout; normal training/evaluation limits stay unchanged.
The resulting `preflight.json` records tool and environment hashes. This checks
the operator tools; it does not evaluate model ability or start model training.

Run the tool check independently:

```powershell
& ./Preflight.ps1
```

`StartTrial.ps1` starts the development clock only after preflight passes. It
requests `default_permissions=":workspace"` explicitly and verifies the recorded CLI
`turn_context` on startup and each resume using `session_check.py`. The current CLI
rejects combining `--approve-for-me` with `--sandbox`; the legacy `sandbox_mode`
override alone also left the recorded context read-only. The checker verifies the
model, reasoning, writable trial directory, disabled shell network and automatic
approval reviewer. A model/reasoning/permission mismatch or
failure to obtain the context within 60 seconds interrupts the trial before
evaluation. Requesting a permission is not evidence that it took effect. Short
diagnostic model sessions checked real startup/resume contexts and file writes
during repair verification. Those diagnostics were separate from timed trials.

The previous trial `night56-20261027-2300` is retained as a failed experiment.
Its records are not rewritten. A new trial must use a new name and does not
resume that session or reuse its mining-only notes.

This remains a local ten-opponent experiment, including the explicitly labeled
tscmoo author-source candidate, not an official benchmark.

The permission-profile setting follows the [official configuration reference](https://learn.chatgpt.com/docs/config-file/config-reference).
Completed repair evidence is recorded in
`D:/xjzb/openbw-readiness-20261027/repairs-20261028/REPORT.md`.
