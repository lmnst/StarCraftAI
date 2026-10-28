"""Audit existing tool evidence after discovering the public score-accessor stubs."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
from batch import RUN, REPO, WORKSPACE, replay_check, save, sha

old = RUN / "acceptance-025125"
regression = RUN / "cap-regression-unscored"
practice = RUN / "tool-smoke/practice/batch-000"
config = json.loads((RUN / "environment.json").read_text())
output = RUN / "tool-acceptance-final"
output.mkdir(exist_ok=False)
corrections = []
# Keep every original record; write explicit corrections alongside the old data.
for path in RUN.glob("**/result.json"):
    r = json.loads(path.read_text())
    if r.get("reason") == "frame_cap_own_scores":
        correction = {"original_record": str(path), "original_result": r["result"],
                      "corrected_status": "frame_cap_unscored", "corrected_result": "unscored",
                      "reason": "Pinned public OpenBW allKillScore/allRazingScore return 0; raw API 0:0 does not establish a draw"}
        save(path.with_name("scoring-correction.json"), correction)
        corrections.append(correction)
save(output / "scoring-corrections.json", corrections)
speed = {str(w): json.loads((old / f"speed-w{w}/summary.json").read_text()) for w in (1, 2, 4)}
checks = {"serial_and_parallel_natural_ends": all(s["valid"] == 6 for s in speed.values()),
          "parallel_speedup_ge_1_5": speed["1"]["wall_seconds"] / speed["4"]["wall_seconds"] >= 1.5,
          "parallel_throughput_ge_6_games_per_minute": speed["4"]["games_per_minute"] >= 6}
for w in (1, 2, 4):
    manifest = json.loads((old / f"speed-w{w}/manifest.json").read_text())
    for p, expected in manifest["binary_hashes"].items():
        if sha(Path(p)) != expected:
            raise RuntimeError(f"Changed benchmark dependency: {p}")
smoke = json.loads((practice / "summary.json").read_text())
checks["one_shot_build_practice_summary"] = smoke["valid"] == 6 and smoke["unresolved"] == 0 and (practice / "feedback.txt").stat().st_size < 32000
cap = json.loads((regression / "summary.json").read_text())
cap_records = [json.loads(p.read_text()) for p in regression.glob("*/result.json")]
checks["unsupported_cap_scores_never_count_as_draws"] = cap["unscored"] == 3 and cap["draws"] == 0 and cap["unresolved"] == 0
checks["cap_replays_play_to_end"] = len(cap_records) == 3 and all(r["replay_check"]["passed"] for r in cap_records)
checks["86400_cap_observed_both_sides"] = all(
    [json.loads(line) for line in p.read_text().splitlines() if json.loads(line)["event"] == "cap"][0]["frame"] == 86400
    for p in (old / "real-frame-cap").glob("g*/player*/referee.jsonl"))
natural_output = output / "natural-replays"
natural_output.mkdir()
def check_natural(i):
    return replay_check(config, old / f"speed-w4/g{i:03}/game.rep", natural_output / f"g{i:03}", 180)
with ThreadPoolExecutor(max_workers=3) as pool:
    natural = list(pool.map(check_natural, range(3)))
checks["three_map_natural_replays_play_to_end"] = all(r["passed"] for r in natural)
report = {"kind": "practice_tool_acceptance_only", "passed": all(checks.values()), "checks": checks,
          "selected_workers": 4, "speed": speed,
          "speedup": round(speed["1"]["wall_seconds"] / speed["4"]["wall_seconds"], 3),
          "speed_artifacts_reused_without_rerunning_games": str(old), "one_shot_practice": smoke,
          "cap_regression": cap, "artifact_directory": str(output),
          "official_scoring_ready": False, "all_13_fixtures_checked": False,
          "independent_trial_started": False, "held_out_evaluation_started": False,
          "limitations": ["Public OpenBW kill+razing accessors are unimplemented. Capped games remain unscored, never inferred draws",
                          "Four practice fixtures only; official roster and exact versions are not qualified",
                          "candidate_host denotes requested launch order, not proven engine host or guaranteed balanced spawn positions"],
          "score_backend_evidence": "upstream/bwapi/bwapi/OpenBWData/BW/BWData.cpp:1785 and :1795",
          "environment_sha256": sha(RUN / "environment.json"),
          "runtime_hashes": {str(p): sha(p) for p in (WORKSPACE / "build/bwapi/lib").glob("*.so")},
          "scope": "local_openbw_not_official"}
save(output / "acceptance.json", report)
save(RUN / "acceptance.json", report)
print(json.dumps(report), flush=True)
raise SystemExit(0 if report["passed"] else 1)
