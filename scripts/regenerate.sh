#!/bin/bash
# Regenerates RecompiledFuncs/ and rsp/aspMain.cpp from the ROM.
#
# The recompiler inputs (splat config, symbols, N64Recomp config) live in recomp/. The N64Recomp
# and RSPRecomp binaries come from an N64Recomp checkout, by default ../N64Recomp next to this
# project (override with N64RECOMP_DIR).
#
# Usage: scripts/regenerate.sh
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RECOMP_DIR="$PROJECT_DIR/recomp"
N64RECOMP_DIR="$(cd "${N64RECOMP_DIR:-$PROJECT_DIR/../N64Recomp}" && pwd)"
N64RECOMP="$N64RECOMP_DIR/N64Recomp"
RSPRECOMP="$N64RECOMP_DIR/build-tools/RSPRecomp"

cd "$RECOMP_DIR"

if [ ! -f baserom.z64 ]; then
    echo "Put your ROM at recomp/baserom.z64 (big-endian .z64)." >&2
    exit 1
fi
if [ ! -x "$N64RECOMP" ]; then
    echo "N64Recomp not found at $N64RECOMP (set N64RECOMP_DIR)." >&2
    exit 1
fi

echo "== Building the working ROM (retail ROM + decompressed code modules)"
python3 tools/build_decomp_rom.py baserom.z64 kengriffey_decomp.z64

echo "== Splitting with splat"
splat split kengriffeyslugfest.yaml > splat.log 2>&1 || { tail -20 splat.log; exit 1; }

echo "== Building the ELF"
./N64MakeElf.sh kengriffeyslugfest.z64
if ! cmp -s build/kengriffeyslugfest.z64 kengriffey_decomp.z64; then
    echo "Rebuilt ROM doesn't match kengriffey_decomp.z64" >&2
    exit 1
fi

echo "== Recompiling CPU code"
rm -f "$PROJECT_DIR"/RecompiledFuncs/*
mkdir -p "$PROJECT_DIR/RecompiledFuncs"
"$N64RECOMP" kengriffey.toml

echo "== Recompiling the audio microcode"
if [ ! -x "$RSPRECOMP" ]; then
    cmake -S "$N64RECOMP_DIR" -B "$N64RECOMP_DIR/build-tools" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build "$N64RECOMP_DIR/build-tools" --target RSPRecomp
fi
cd "$PROJECT_DIR"
"$RSPRECOMP" aspMain.us.toml

echo "Done."
