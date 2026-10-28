"""One-shot tool acceptance; never launches a model or the held-out schedule."""
import argparse
import json
from pathlib import Path
import time
from batch import RUN, WORKSPACE, batch, save, sha

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--all-fixtures", action="store_true", help="Also qualify all 13 opponents on 3 maps before a future timed trial")
args = parser.parse_args()
config = json.loads((RUN / "environment.json").read_text())
if args.all_fixtures and len(config["opponents"]) != 13:
    raise RuntimeError("Full fixture qualification requires 13 distinct configured opponents")
directory = RUN / ("acceptance-" + time.strftime("%H%M%S", time.gmtime()))
directory.mkdir()
schedule = json.loads((RUN / "acceptance-schedule.json").read_text())
# The acceptance schedule uses the original infrastructure reference name.
for item in schedule:
    item["opponent"] = "gpt-6-astra-1"
checks = {}
speed = {}
for workers in (1, 2, 4):
    speed[workers] = batch(config, WORKSPACE / "build/bot/baseline.so", schedule,
                           directory / f"speed-w{workers}", workers=workers)
    print(json.dumps({"speed_workers": workers, "result": speed[workers]}), flush=True)
checks["speed_all_games_valid"] = all(s["valid"] == 6 for s in speed.values())
parallel = max((2, 4), key=lambda w: speed[w]["games_per_minute"])
checks["parallel_speedup_ge_1_5"] = speed[1]["wall_seconds"] / speed[parallel]["wall_seconds"] >= 1.5
checks["parallel_at_least_6_games_per_minute"] = speed[parallel]["games_per_minute"] >= 6
checks["pair_rss_under_512_mib"] = all(s["peak_pair_rss_mib"] < 512 for s in speed.values())
preflight_schedule = json.loads((RUN / "preflight-schedule.json").read_text())
if not args.all_fixtures:
    preflight_schedule = [item for item in preflight_schedule if item["opponent"] in ("Steamhammer-PublicSource", "Locutus-PublicSource")]
preflight = batch(config, RUN / "harness/miner.so", preflight_schedule,
                  directory / "fixture-smoke", workers=parallel, frame_cap=2400, timeout=240)
checks["selected_fixture_smoke"] = preflight["valid"] + preflight["unscored"] == len(preflight_schedule) and preflight["unresolved"] == 0
# A replay for every map and host role, with a short forced cap.
caps = batch(config, RUN / "harness/miner.so", schedule, directory / "cap-and-replays",
             workers=parallel, frame_cap=2400, verify=True)
checks["short_cap_retained_unscored_with_replays"] = caps["unscored"] == 6 and caps["unresolved"] == 0 and caps["draws"] == 0
# Exercise the real cap, not only the shortened preflight cap.
cap_config = dict(config, opponents=[{"name": "MinerAcceptanceOnly", "race": "Protoss", "binary": str(RUN / "harness/miner.so")}])
full_schedule = [dict(schedule[i], opponent="MinerAcceptanceOnly") for i in range(2)]
full = batch(cap_config, RUN / "harness/miner.so", full_schedule, directory / "real-frame-cap",
             workers=parallel, verify=True)
checks["real_86400_cap_retained_unscored_with_replay"] = full["unscored"] == 2 and full["unresolved"] == 0 and full["draws"] == 0
asymmetric = batch(config, WORKSPACE / "build/bot/baseline.so", schedule,
                   directory / "asymmetric-cap", workers=parallel, frame_cap=9000)
records = [json.loads(p.read_text()) for p in (directory / "asymmetric-cap").glob("*/result.json")]
checks["unsupported_scores_never_invent_a_draw"] = asymmetric["unscored"] == 6 and asymmetric["draws"] == 0 and asymmetric["unresolved"] == 0
config["practice_workers"] = parallel
save(RUN / "environment.json", config)
report = {"passed": all(checks.values()), "checks": checks, "selected_workers": parallel,
          "kind": "practice_tool_acceptance_only", "all_13_fixtures_checked": args.all_fixtures,
          "independent_trial_started": False, "held_out_evaluation_started": False,
          "official_scoring_ready": False,
          "limitations": ["Public OpenBW kill and razing accessors are unimplemented; capped games remain unscored", "Official opponent roster and versions are not qualified"],
          "speedup": round(speed[1]["wall_seconds"] / speed[parallel]["wall_seconds"], 3),
          "speed": speed, "artifact_directory": str(directory),
          "source": "tools/openbw/acceptance.py", "environment_sha256": sha(RUN / "environment.json"),
          "preflight": preflight, "scope": "local_openbw_not_official"}
save(directory / "acceptance.json", report)
save(RUN / "acceptance.json", report)
print(json.dumps(report), flush=True)
raise SystemExit(0 if report["passed"] else 1)
