"""Configure four practice-tool fixtures. This is not an evaluation roster."""
import json
from pathlib import Path
import shutil
import subprocess
from batch import RUN, REPO, WORKSPACE, save, sha

config = json.loads((RUN / "environment.json").read_text())
source = REPO / "benchmark/openbw-20261027/sources/StardustDevEnvironment"
opponents = []
for name in ("Steamhammer", "Locutus"):
    fixture = RUN / "fixtures" / name / "bwapi-data"
    (fixture / "AI/bwta").mkdir(parents=True, exist_ok=True)
    for sub in ("read", "write"):
        (fixture / sub).mkdir(exist_ok=True)
    file = "Steamhammer_2.4.2.json" if name == "Steamhammer" else "Locutus.json"
    shutil.copyfile(source / "test/3rdparty/opponents" / name / file, fixture / "AI" / file)
    shutil.copytree(source / "test/3rdparty/bwta", fixture / "AI/bwta", dirs_exist_ok=True)
    opponents.append({"name": name + "-PublicSource", "race": "Protoss", "kind": "human_source",
                      "binary": str(RUN / "opponents" / (name + ".so")), "fixture": str(fixture),
                      "source": "https://github.com/bmnielsen/StardustDevEnvironment",
                      "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip(),
                      "official_version_match": "unconfirmed"})
for version, filename in ((1, "reference-tier-d.so"), (5, "reference-tier-s.so")):
    opponents.append({"name": f"gpt-6-astra-{version}", "race": "Protoss", "kind": "tool_acceptance_reference_only",
                      "binary": str(WORKSPACE / "build/bot" / filename),
                      "source": "https://github.com/llmskirmish/starskirmish-hillclimb",
                      "source_commit": config["sources"]["starskirmish-hillclimb"],
                      "official_opponent": False})
for opponent in opponents:
    opponent["binary_sha256"] = sha(Path(opponent["binary"]))
config["opponents"] = opponents
save(RUN / "environment.json", config)
save(RUN / "roster.json", [{k: v for k, v in o.items() if k not in ("binary", "fixture")} for o in opponents])
preflight = [{"id": f"p{i:03}", "map": m["path"], "opponent": o["name"],
              "seed": 4000000 + i, "candidate_host": i % 2 == 0}
             for i, (o, m) in enumerate((o, m) for o in opponents for m in config["maps"])]
save(RUN / "preflight-schedule.json", preflight)
print(json.dumps({"practice_fixtures": len(opponents), "held_out_games": 0}))
