#!/bin/bash
# Regenerates RecompiledFuncs/ and rsp/aspMain.cpp from the ROM.
#
# The recompiler inputs (splat config, symbols, N64Recomp config) live in recomp/. N64Recomp and
# RSPRecomp are built from the tools/N64Recomp submodule into build-tools/ on first use.
#
# Usage: scripts/regenerate.sh
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
RECOMP_DIR="$PROJECT_DIR/recomp"
N64RECOMP_SRC="$PROJECT_DIR/tools/N64Recomp"
TOOLS_BUILD="$PROJECT_DIR/build-tools"
N64RECOMP="$TOOLS_BUILD/N64Recomp"
RSPRECOMP="$TOOLS_BUILD/RSPRecomp"

cd "$RECOMP_DIR"

if [ ! -f baserom.z64 ]; then
    echo "Put your ROM at recomp/baserom.z64 (big-endian .z64)." >&2
    exit 1
fi
if [ ! -f "$N64RECOMP_SRC/CMakeLists.txt" ]; then
    echo "tools/N64Recomp is missing; run: git submodule update --init --recursive" >&2
    exit 1
fi
if [ ! -x "$N64RECOMP" ] || [ ! -x "$RSPRECOMP" ]; then
    echo "== Building N64Recomp and RSPRecomp"
    cmake -S "$N64RECOMP_SRC" -B "$TOOLS_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build "$TOOLS_BUILD" --target N64Recomp RSPRecomp
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
cd "$PROJECT_DIR"
"$RSPRECOMP" aspMain.us.toml

echo "Done."
