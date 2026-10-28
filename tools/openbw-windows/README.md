# Windows OpenBW local trial

Current local environment (2026-10-28): ten opponents, three maps, six games per
pair, 180 evaluation games. Scoring and operator preflight have passed local
acceptance. Stardust uses an explicitly labeled source-repair candidate that
passed 18 three-map regression games; the retained release DLL is not repaired.
See the [dated progress record](../../benchmark/windows-openbw-20261028/README.md).

## Local prerequisites

From the repository root, copy and edit the machine configuration:

```powershell
Copy-Item ./local-paths.example.json ./local-paths.json
# Set each value to an existing local dependency path before running tools.
```

`local-paths.json` is ignored by Git. Absolute paths are accepted; relative paths
are resolved against the repository root, independently of the working directory.
Set `STARCRAFTAI_PATHS` to use another configuration file. All configured paths
refer to existing dependencies; the example does not download or install them.

| Setting | Required content |
| --- | --- |
| `runtime_root` | Accepted `environment.json`, acceptance records, opponent runtimes, maps, engines and `sdk/` containing FAP/BWEM |
| `probe_root` | Existing probe fixture with `player1/` MPQ data and `engine/bin/Release/` |
| `python` | Python executable |
| `cmake`, `msbuild` | CMake and VS 2017 MSBuild executables; current build uses v141, Win32 and SDK 10.0.17763.0 |
| `bwapi_include`, `bwapi_library` | Matching BWAPI headers and `BWAPILIB.lib` |
| `bwapi_ini` | BWAPI INI template |
| `workspace_root` | Engine workspace for the archived WSL tools, with `upstream/`, `build/`, `assets/` and `.tools/` |

The match backend is included as `match_backend.py`. A locally configured Codex
CLI is required for timed trials. Runtime manifests contain paths and hashes of
their own; moving dependencies also requires preparing and accepting a new
runtime configuration. Do not rewrite manifests or acceptance records of old trials.

For the archived WSL tools, use `STARCRAFTAI_PATHS` pointing to a separate JSON
configuration whose `workspace_root` is a Linux path, such as `/mnt/d/engine-workspace`.
Those Python/Bash tools share the same resolver; Windows paths are not converted
implicitly. Native `tools/local234/Build.ps1` uses the configured `msbuild`.

Third-party source trees, binary dependencies, model session logs and game replays
are not distributed in this repository and still need to be prepared separately.

## Development and evaluation

The objective is match win rate. `Starter.cpp` is a replaceable minimal starting
point; its mining behavior does not restrict construction, army production or
combat. `task.md` supplies the full training instructions. Resume prompts repeat
the objective instead of relying only on the agent's saved notes.

Before starting a model, `StartTrial.ps1` runs `Preflight.ps1`. This creates an
independent directory under `<runtime_root>/preflight/`, writes a
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
`<runtime_root>/repairs-20261028/REPORT.md`.
