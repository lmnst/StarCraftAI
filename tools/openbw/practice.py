"""One-shot compiler/practice command. Exits after the requested batch."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import subprocess
from batch import RUN, REPO, batch, save, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trial", type=Path, required=True)
    parser.add_argument("--action", choices=("Build", "Practice"), default="Practice")
    parser.add_argument("--opponents", default="")
    parser.add_argument("--games", type=int, default=6)
    args = parser.parse_args()
    trial = args.trial.resolve()
    if not 1 <= args.games <= 12:
        parser.error("Practice batches must contain 1..12 games")
    version = trial / "builds" / datetime.now(timezone.utc).strftime("%H%M%S-%f")
    version.mkdir(parents=True)
    shutil.copytree(trial / "src", version / "src")
    with (version / "compiler.log").open("w") as log:
        p = subprocess.run(["bash", str(REPO / "tools/openbw/build.sh"), str(version), "candidate"], stdout=log, stderr=subprocess.STDOUT)
    result = {"version": str(version), "exit_code": p.returncode,
              "sources": {p.name: sha(p) for p in (version / "src").glob("*") if p.is_file()}}
    if p.returncode:
        save(version / "build.json", result)
        print(f'Build failed. Full log: {version}/compiler.log\n' + (version / "compiler.log").read_text(errors="replace")[-14000:])
        return p.returncode
    result["binary_sha256"] = sha(version / "candidate.so")
    save(version / "build.json", result)
    shutil.copyfile(version / "candidate.so", trial / "candidate.so")
    if args.action == "Build":
        print(json.dumps(result))
        return 0
    config = json.loads((RUN / "environment.json").read_text())
    names = args.opponents.split(",") if args.opponents else [o["name"] for o in config["opponents"]]
    if not all(n in {o["name"] for o in config["opponents"]} for n in names):
        parser.error("Unknown opponent; see roster.json")
    serial = len(list((trial / "practice").glob("*"))) if (trial / "practice").exists() else 0
    schedule = [{"id": f"g{i:03}", "map": config["maps"][i % 3]["path"],
                 "opponent": names[(serial * args.games + i) % len(names)],
                 "seed": 1000000 + serial * 100 + i, "candidate_host": i % 2 == 0}
                for i in range(args.games)]
    output = trial / "practice" / f"batch-{serial:03}"
    batch(config, version / "candidate.so", schedule, output, workers=config["practice_workers"], timeout=240)
    print(f'Practice artifacts: {output}\n' + (output / "feedback.txt").read_text(), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
