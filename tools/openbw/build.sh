#!/usr/bin/env bash
set -euo pipefail
tools=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$tools/../.." && pwd)
workspace=$(python3 -c 'import sys; sys.path.insert(0, sys.argv[1]); from local_paths import load_paths; print(load_paths()["workspace_root"])' "$tools/..")
target=${1:-"$repo/runs/openbw-20261027/harness"}
mkdir -p "$target"
bwapi_headers="$workspace/upstream/bwapi/bwapi/include"
if [[ -f "$repo/runs/openbw-20261027/opponents/compile-cache.txt" ]]; then
  cache=$(cat "$repo/runs/openbw-20261027/opponents/compile-cache.txt")
  if [[ "$cache" == */.cache/starcraft-openbw-20261027 && -f "$cache/bwapi/bwapi/include/BWAPI.h" ]]; then
    bwapi_headers="$cache/bwapi/bwapi/include"
  fi
fi
common=(-std=c++20 -O2 -fPIC -shared -include cstdint
  -isystem "$bwapi_headers"
  -L "$workspace/build/bwapi/lib")
if [[ ${2:-} == candidate ]]; then
  # Output is a new snapshot; running practice never holds the build output.
  g++ "${common[@]}" -isystem "$workspace/upstream/starskirmish-hillclimb/libs/fap/FAP/include" \
    -isystem "$workspace/upstream/starskirmish-hillclimb/libs/bwem/BWEM/include" \
    "$target/src/"*.cpp -L "$workspace/upstream/starskirmish-hillclimb/build/bwem" -lBWEM -lBWAPILIB \
    -o "$target/candidate.next.so"
  mv "$target/candidate.next.so" "$target/candidate.so"
else
  g++ "${common[@]}" "$tools/Referee.cpp" -lBWAPILIB -o "$target/referee.so"
  g++ "${common[@]}" "$tools/Miner.cpp" -lBWAPILIB -o "$target/miner.so"
fi
