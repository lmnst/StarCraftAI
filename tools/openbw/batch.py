#!/usr/bin/env python3
"""Isolated headless OpenBW matches and bounded practice feedback (local only)."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
import sys
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))
from local_paths import load_paths
WORKSPACE = load_paths()["workspace_root"]
RUN = REPO / "runs/openbw-20261027"


def save(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def events(path):
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text().splitlines()]


def side_summary(trace):
    samples = [e for e in trace if e["event"] == "sample"]
    first = {}
    created = {}
    losses = Counter()
    windows = []
    for e in trace:
        if e["event"] == "complete":
            first.setdefault(e["type"], e["frame"])
        elif e["event"] == "create":
            created.setdefault(e["type"], e["frame"])
        elif e["event"] == "destroy":
            losses[e["type"]] += 1
            if not windows or e["frame"] - windows[-1]["last_frame"] > 240:
                windows.append({"first_frame": e["frame"], "last_frame": e["frame"], "own_losses": {}})
            window = windows[-1]
            window["last_frame"] = e["frame"]
            window["own_losses"][e["type"]] = window["own_losses"].get(e["type"], 0) + 1
    # These are sampled observations, not an inferred causal diagnosis.
    return {"first_creation_frames": created, "first_completion_frames": first,
            "own_units_lost": dict(losses), "own_loss_windows": windows,
            "samples": len(samples),
            "supply_full_samples": sum(e["supply_used"] >= e["supply_total"] and e["supply_total"] < 400 for e in samples),
            "minerals_ge_1000_samples": sum(e["minerals"] >= 1000 for e in samples),
            "idle_worker_samples": sum(e["idle_workers"] > 0 for e in samples),
            "last_economy": samples[-1] if samples else None}


def replay_check(config, replay, directory, timeout):
    directory.mkdir()
    (directory / "bwapi-data").mkdir()
    shutil.copyfile(config["ini"], directory / "bwapi-data/bwapi.ini")
    env = base_env(config) | {
        "BWAPI_CONFIG_AUTO_MENU__AUTO_MENU": "SINGLE_PLAYER",
        "BWAPI_CONFIG_AUTO_MENU__MAP": str(replay),
        "BWAPI_CONFIG_AUTO_MENU__SAVE_REPLAY": "",
        "BWAPI_CONFIG_AI__AI": config["replay_checker"],
        "BWAPI_CONFIG_AI__TOURNAMENT": "",
    }
    with (directory / "playback.log").open("w") as log:
        p = subprocess.Popen([config["launcher"]], cwd=directory, env=env, stdout=log,
                             stderr=subprocess.STDOUT, start_new_session=True)
        try:
            p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(p.pid, signal.SIGKILL)
            p.wait()
    raw = (directory / "playback.log").read_text(errors="replace").splitlines()
    trace = [json.loads(line) for line in raw if line.startswith('{"event":')]
    start = next((e for e in trace if e["event"] == "replay_start"), None)
    end = next((e for e in trace if e["event"] == "replay_end"), None)
    ok = (p.returncode == 0 and start and start["is_replay"] and end and
          end["frames"] >= start["expected_frames"] - 1)
    report = {"passed": bool(ok), "returncode": p.returncode, "events": trace}
    save(directory / "verification.json", report)
    return report


def base_env(config):
    return os.environ | {
        "LD_LIBRARY_PATH": config["library_dir"], "OPENBW_MPQ_PATH": config["mpq"],
        "OPENBW_ENABLE_UI": "0", "OPENBW_LAN_MODE": "LOCAL_AUTO",
        "BWAPI_CONFIG_AUTO_MENU__AUTO_MENU": "LAN",
        "BWAPI_CONFIG_AUTO_MENU__GAME_TYPE": "MELEE",
        "BWAPI_CONFIG_AUTO_MENU__AUTO_RESTART": "OFF",
        "BWAPI_CONFIG_STARCRAFT__SPEED_OVERRIDE": "0",
        "BWAPI_CONFIG_STARCRAFT__SOUND": "OFF",
        "BWAPI_CONFIG_AI__TOURNAMENT": config["referee"],
    }


def play(config, candidate, item, directory, frame_cap, timeout, verify_replay):
    directory.mkdir()
    replay = directory / "game.rep"
    record = dict(item, scope="local_openbw_not_official", status="starting")
    opponent = next(o for o in config["opponents"] if o["name"] == item["opponent"])
    players = [(candidate, "Protoss", None), (Path(opponent["binary"]), opponent["race"], opponent.get("fixture"))]
    candidate_side = 0 if item["candidate_host"] else 1
    if candidate_side == 1:
        players.reverse()
    procs, logs = [], []
    started = time.monotonic()
    peak_rss = 0
    try:
        with tempfile.TemporaryDirectory(prefix="openbw-") as sockets:
            env = base_env(config) | {
                "OPENBW_LOCAL_AUTO_DIRECTORY": sockets,
                "BWAPI_CONFIG_AUTO_MENU__MAP": item["map"],
                "BWAPI_CONFIG_STARCRAFT__SEED_OVERRIDE": str(item["seed"]),
                "STARBENCH_FRAME_CAP": str(frame_cap),
            }
            try:
                for side, (bot, race, fixture) in enumerate(players):
                    work = directory / f"player{side + 1}"
                    work.mkdir()
                    if fixture:
                        shutil.copytree(fixture, work / "bwapi-data")
                    for sub in ("read", "write", "AI"):
                        (work / "bwapi-data" / sub).mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(config["ini"], work / "bwapi-data/bwapi.ini")
                    player_env = env | {
                        "BWAPI_CONFIG_AI__AI": str(bot),
                        "BWAPI_CONFIG_AUTO_MENU__RACE": race,
                        "BWAPI_CONFIG_AUTO_MENU__CHARACTER_NAME": f"player{side + 1}",
                        "BWAPI_CONFIG_AUTO_MENU__SAVE_REPLAY": str(replay) if side == 0 else "",
                    }
                    log = (directory / f"player{side + 1}.log").open("w")
                    logs.append(log)
                    procs.append(subprocess.Popen([config["launcher"]], cwd=work, env=player_env,
                                                  stdout=log, stderr=subprocess.STDOUT, start_new_session=True))
                    if side == 0:
                        time.sleep(0.5)
                while any(p.poll() is None for p in procs):
                    rss = 0
                    for p in procs:
                        try:
                            for line in Path(f"/proc/{p.pid}/status").read_text().splitlines():
                                if line.startswith("VmRSS:"):
                                    rss += int(line.split()[1])
                        except FileNotFoundError:
                            pass
                    peak_rss = max(peak_rss, rss)
                    if time.monotonic() - started > timeout:
                        raise subprocess.TimeoutExpired("OpenBW match", timeout)
                    time.sleep(0.05)
                record["status"] = "ended"
            except subprocess.TimeoutExpired:
                record["status"] = "wall_timeout_unresolved"
            except Exception as error:
                record.update(status="infrastructure_error", error=str(error))
            finally:
                for p in procs:
                    if p.poll() is None:
                        os.killpg(p.pid, signal.SIGKILL)
                    p.wait()
                for log in logs:
                    log.close()
    finally:
        record.update(seconds=round(time.monotonic() - started, 3),
                      returncodes=[p.returncode for p in procs], peak_pair_rss_kib=peak_rss)
    traces = [events(directory / f"player{s + 1}/referee.jsonl") for s in range(2)]
    ends = [[e for e in t if e["event"] == "end"] for t in traces]
    caps = [[e for e in t if e["event"] == "cap"] for t in traces]
    record["starts"] = [[e for e in t if e["event"] == "start"] for t in traces]
    record["feedback"] = side_summary(traces[candidate_side])
    if record["status"] == "ended":
        if record["returncodes"] != [0, 0]:
            record["status"] = "process_failure_unresolved"
        elif not all(ends) or not replay.is_file() or replay.stat().st_size == 0:
            record["status"] = "missing_end_or_replay"
        elif any(caps):
            if not all(caps) or caps[0][-1]["frame"] != caps[1][-1]["frame"]:
                record["status"] = "cap_evidence_mismatch"
            else:
                scores = [caps[s][-1]["score"] for s in (candidate_side, 1 - candidate_side)]
                if not config.get("kill_razing_scores_supported", False):
                    record.update(status="frame_cap_unscored", reason="public_openbw_score_accessors_are_stubs",
                                  raw_api_scores=scores, result="unscored")
                else:
                    record.update(status="valid", reason="frame_cap_own_scores", scores=scores,
                                  result="win" if scores[0] > scores[1] else "loss" if scores[0] < scores[1] else "draw")
        elif ends[0][-1]["winner"] == ends[1][-1]["winner"]:
            record["status"] = "noncomplementary_end"
        else:
            record.update(status="valid", reason="complementary_natural_end",
                          result="win" if ends[candidate_side][-1]["winner"] else "loss")
        record["frames"] = [es[-1]["frame"] if es else None for es in ends]
    if replay.exists():
        record.update(replay_sha256=sha(replay), replay_bytes=replay.stat().st_size)
    if verify_replay and record["status"] in ("valid", "frame_cap_unscored"):
        record["replay_check"] = replay_check(config, replay, directory / "replay-check", timeout)
        if not record["replay_check"]["passed"]:
            record["status"] = "replay_verification_failed"
    save(directory / "result.json", record)
    return record


def batch(config, candidate, schedule, output, workers=4, frame_cap=86400, timeout=180, verify=False):
    output.mkdir(parents=True, exist_ok=False)
    frozen = output / "candidate.so"
    shutil.copyfile(candidate, frozen)
    save(output / "manifest.json", {
        "created_utc": datetime.now(timezone.utc).isoformat(), "scope": "local_openbw_not_official",
        "workers": workers, "frame_cap": frame_cap, "candidate_sha256": sha(frozen),
        "config": config, "schedule": schedule,
        "binary_hashes": {p: sha(Path(p)) for p in
                          [config["launcher"], config["referee"]] + [o["binary"] for o in config["opponents"]]},
    })
    started = time.monotonic()
    results = []
    with (output / "results.jsonl").open("w") as stream, ThreadPoolExecutor(max_workers=workers) as pool:
        jobs = [pool.submit(play, config, frozen, item, output / item["id"], frame_cap, timeout, verify) for item in schedule]
        for future in as_completed(jobs):
            r = future.result()
            results.append(r)
            stream.write(json.dumps(r) + "\n"); stream.flush()
    valid = [r for r in results if r["status"] == "valid"]
    unscored = [r for r in results if r["status"] == "frame_cap_unscored"]
    wall = time.monotonic() - started
    summary = {"scope": "local_openbw_not_official", "requested": len(schedule), "valid": len(valid),
               "wins": sum(r.get("result") == "win" for r in valid),
               "losses": sum(r.get("result") == "loss" for r in valid),
               "draws": sum(r.get("result") == "draw" for r in valid),
               "unscored": len(unscored), "unresolved": len(schedule) - len(valid) - len(unscored), "workers": workers,
               "wall_seconds": round(wall, 3), "games_per_minute": round(len(valid) * 60 / wall, 3),
               "peak_pair_rss_mib": round(max((r["peak_pair_rss_kib"] for r in results), default=0) / 1024, 1),
               "failures": [{"id": r["id"], "status": r["status"]} for r in results if r["status"] not in ("valid", "frame_cap_unscored")]}
    summary["score"] = summary["wins"] / 234 * 100 if len(schedule) == 234 and len(valid) == 234 and frame_cap == 86400 else None
    save(output / "summary.json", summary)
    (output / "feedback.txt").write_text(feedback_text(summary, results))
    return summary


def feedback_text(summary, results):
    lines = [json.dumps(summary, ensure_ascii=False), "Sampled feedback (frames / 24 = approximate game seconds; loss windows contain own losses only):"]
    for r in sorted(results, key=lambda r: r["id"]):
        f = r["feedback"]
        lines.append(f'{r["id"]} {r["opponent"]} {r.get("result", r["status"])} '
                     f'{r.get("frames")} frames; created={json.dumps(f["first_creation_frames"])}; '
                     f'completed={json.dumps(f["first_completion_frames"])}; '
                     f'lost={json.dumps(f["own_units_lost"])}; full_supply={f["supply_full_samples"]}/{f["samples"]}; '
                     f'bank>=1000={f["minerals_ge_1000_samples"]}; idle_workers={f["idle_worker_samples"]}')
        windows = sorted(f["own_loss_windows"], key=lambda w: sum(w["own_losses"].values()), reverse=True)[:3]
        lines.append(f'  largest_own_loss_windows={json.dumps(windows)}')
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, default=RUN / "environment.json")
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--schedule", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--frame-cap", type=int, default=86400)
    parser.add_argument("--timeout", type=float, default=180)
    parser.add_argument("--verify-replays", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.workers <= 6 or not 1 <= args.frame_cap <= 86400:
        parser.error("workers must be 1..6; frame cap must be 1..86400")
    summary = batch(json.loads(args.config.read_text()), args.candidate.resolve(),
                    json.loads(args.schedule.read_text()), args.output.resolve(), args.workers,
                    args.frame_cap, args.timeout, args.verify_replays)
    print(json.dumps(summary), flush=True)
    return 0 if not summary["unresolved"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
