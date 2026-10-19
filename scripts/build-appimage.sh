#!/bin/bash
# Builds a Linux AppImage in an Ubuntu 22.04 container (packaging/linux/Dockerfile), so it runs on
# SteamOS and other distributions with glibc 2.35 or newer.
#
# Needs Docker, and the recompiled code from scripts/regenerate.sh (RecompiledFuncs/ and
# rsp/aspMain.cpp); they're built from your ROM and aren't in git, so the container uses the ones in
# this checkout. The ROM itself isn't packaged.
#
# Usage: scripts/build-appimage.sh   -> dist/SlugfestRecompiled-<version>-x86_64.AppImage
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE=slugfest-recomp-appimage
VERSION="$(sed -n 's/^const std::string version_string = "\(.*\)";/\1/p' "$PROJECT_DIR/src/main/main.cpp")"

if [ ! -d "$PROJECT_DIR/RecompiledFuncs" ] || [ ! -f "$PROJECT_DIR/rsp/aspMain.cpp" ]; then
    echo "Run scripts/regenerate.sh first (RecompiledFuncs/ and rsp/aspMain.cpp are missing)." >&2
    exit 1
fi

echo "== Building the container image"
docker build -t "$IMAGE" "$PROJECT_DIR/packaging/linux"

echo "== Building SlugfestRecompiled $VERSION"
mkdir -p "$PROJECT_DIR/dist"
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$PROJECT_DIR:/src" -w /src "$IMAGE" bash -euo pipefail -c "
    # -include string: RecompFrontend's recompinput.h uses std::string without including it, which
    # newer standard libraries happen to cover.
    cmake -S . -B build-appimage -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
        -DCMAKE_CXX_FLAGS='-include string'

    cmake --build build-appimage -j\$(nproc)

    rm -rf build-appimage/AppDir
    mkdir -p build-appimage/AppDir/usr/bin
    cp build-appimage/SlugfestRecompiled build-appimage/AppDir/usr/bin/
    strip build-appimage/AppDir/usr/bin/SlugfestRecompiled
    cp -r assets recompcontrollerdb.txt build-appimage/AppDir/usr/bin/
    cp LICENSE build-appimage/AppDir/usr/bin/

    cd build-appimage
    export OUTPUT=SlugfestRecompiled-$VERSION-x86_64.AppImage
    export DEPLOY_GTK_VERSION=3
    linuxdeploy --appdir AppDir \
        -e AppDir/usr/bin/SlugfestRecompiled \
        -d ../packaging/linux/SlugfestRecompiled.desktop \
        -i ../icons/app.png --icon-filename SlugfestRecompiled \
        --custom-apprun ../packaging/linux/AppRun \
        --plugin gtk --output appimage
    mv \"\$OUTPUT\" ../dist/
"

echo "== Done: dist/SlugfestRecompiled-$VERSION-x86_64.AppImage"
