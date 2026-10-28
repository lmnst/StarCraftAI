#!/usr/bin/env bash
set -euo pipefail
tools=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd "$tools/../.." && pwd)
root=$(cd "$repo/../.." && pwd)
out="$repo/runs/openbw-20261027/opponents"
mkdir -p "$out"
cache="$HOME/.cache/starcraft-openbw-20261027"
mkdir -p "$cache"
if [[ ! -f "$cache/dev/test/3rdparty/opponents/Steamhammer/Steamhammer/Source/ParseUtils.cpp" ]]; then
python3 - "$out" "$cache" <<'PY'
import sys, zipfile
from pathlib import Path
out, cache = map(Path, sys.argv[1:])
with zipfile.ZipFile(out / 'human-sources.zip') as z:
    z.extractall(cache / 'dev/test/3rdparty/opponents')
with zipfile.ZipFile(out / 'bwapi-headers.zip') as z:
    z.extractall(cache / 'bwapi/bwapi/include')
PY
fi
printf '%s\n' "$cache" > "$out/compile-cache.txt"
python3 - "$cache" "$out" <<'PY'
import difflib, sys
from pathlib import Path
cache, out = map(Path, sys.argv[1:])
p = cache / 'dev/test/3rdparty/opponents/Steamhammer/Steamhammer/Source/ParseUtils.cpp'
text = p.read_text()
fixed = text.replace('std::vector<const std::string> openingNames;', 'std::vector<std::string> openingNames;')
if fixed != text:
    (out / 'steamhammer-gcc-port.patch').write_text(''.join(difflib.unified_diff(text.splitlines(True), fixed.splitlines(True), fromfile='ParseUtils.cpp', tofile='ParseUtils.cpp')))
    p.write_text(fixed)
PY
cmake="$root/.tools/cmake-3.31.6-linux-x86_64/bin/cmake"
"$cmake" -S "$tools/humans" -B "$cache/build" -DCMAKE_BUILD_TYPE=Release \
  -DENGINE_SOURCE="$cache/bwapi" -DENGINE_BUILD="$root/build/bwapi" \
  -DOPPONENT_SOURCE="$cache/dev"
"$cmake" --build "$cache/build" --parallel 4 > "$out/human-build.log" 2>&1
for name in Locutus Steamhammer; do cp "$cache/build/$name/lib$name.so" "$out/$name.so"; done
