"""Create a separate, pinned local OpenBW fixture; never touches native results."""
import json
from pathlib import Path
import shutil
import subprocess
from batch import REPO, WORKSPACE, RUN, save, sha

RUN.mkdir(parents=True, exist_ok=True)
fixtures = RUN / "fixtures"
(fixtures / "maps").mkdir(parents=True, exist_ok=True)
native = json.loads((REPO / "runs/local234-20261026/protocol.json").read_text(encoding="utf-8-sig"))
maps = []
for m in native["maps"]:
    matches = list((REPO / "runs/local234-20261026/runtime").glob("*/maps/**/" + m["name"]))
    source = next((p for p in matches if sha(p).lower() == m["sha256"].lower()), None)
    if source is None:
        raise RuntimeError(f"Missing verified map {m['name']}")
    target = fixtures / "maps" / m["name"]
    shutil.copyfile(source, target)
    maps.append({"name": m["name"], "path": str(target), "sha256": sha(target)})
opponents = [{"name": "PublicAstraTierD", "race": "Protoss", "binary": str(WORKSPACE / "build/bot/reference-tier-d.so")},
             {"name": "PublicAstraTierS", "race": "Protoss", "binary": str(WORKSPACE / "build/bot/reference-tier-s.so")}]
config = {"scope": "local_openbw_not_official", "launcher": str(WORKSPACE / "build/bwapi/bin/BWAPILauncher"),
          "kill_razing_scores_supported": False,
          "library_dir": str(WORKSPACE / "build/bwapi/lib"), "mpq": str(WORKSPACE / "assets/mpq"),
          "ini": str(WORKSPACE / "upstream/bwapi/bwapi.ini.example"),
          "referee": str(RUN / "harness/referee.so"),
          "replay_checker": str(WORKSPACE / "build/bot/replay-check.so"),
          "maps": maps, "opponents": opponents,
          "sources": {name: subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=WORKSPACE / "upstream" / name, text=True).strip()
                      for name in ("bwapi", "openbw", "starskirmish-hillclimb")},
          "differences": ["Public BWAPI port identifies as 4.2.0, official description says 4.4.0",
                          "Exact official opponent versions and official demo sources unavailable",
                          "Local launcher, mining skeleton, telemetry and one-shot practice script",
                          "Single independent run rather than a five-run model average"]}
save(RUN / "environment.json", config)
schedule = [{"id": f"g{i:03}", "map": m["path"], "opponent": "PublicAstraTierD",
             "seed": 2026102700 + i // 2, "candidate_host": i % 2 == 0}
            for i, m in enumerate(maps * 2)]
save(RUN / "acceptance-schedule.json", schedule)
print(json.dumps({"environment": str(RUN / "environment.json"), "maps": maps}))
