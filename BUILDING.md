# Building

Ken Griffey Jr.'s Slugfest: Recompiled is built in two stages: the game's code is recompiled to C, then compiled into the runtime app.

- `recomp/` holds the recompiler inputs: the splat config (`kengriffeyslugfest.yaml`), `symbol_addrs.txt`, `reloc_addrs.txt`, `undefined_syms_manual.txt`, the N64Recomp config (`kengriffey.toml`), the ELF build script and the ROM tools.
- [N64Recomp](https://github.com/N64Recomp/N64Recomp) is a submodule at `tools/N64Recomp`. `scripts/regenerate.sh` builds `N64Recomp` and `RSPRecomp` from it into `build-tools/` the first time it runs.

## Prerequisites (Fedora)

- `clang`, `cmake`, `ninja-build`, `python3`
- `SDL2-devel`, `freetype-devel`, `gtk3-devel` (for the file dialog), Vulkan headers/loader
- For regenerating code: `splat64` (`pip install splat64`) and the MIPS cross binutils (`binutils-mips64-linux-gnu`)
- Your own Ken Griffey Jr.'s Slugfest (USA) ROM, as `recomp/baserom.z64` (big-endian `.z64`, XXH3 `0x7DA2C678136D0287`)

## 1. Get the dependencies

```bash
git submodule update --init --recursive
```

## 2. Generate the recompiled code

```bash
scripts/regenerate.sh
```

This rebuilds the working ROM (the retail ROM with every compressed code module decompressed and appended at `0x1000000`), splits it with splat, links an ELF, checks that the ELF reproduces the working ROM byte for byte, and then runs:

- `N64Recomp kengriffey.toml` → `RecompiledFuncs/`
- `RSPRecomp aspMain.us.toml` → `rsp/aspMain.cpp` (the audio microcode)

Neither output is committed, since both are derived from the ROM.

## 3. Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j"$(nproc)"
```

## 4. Run

Run from the project root, because the UI loads `assets/` and `recompcontrollerdb.txt` relative to the working directory:

```bash
./build/SlugfestRecompiled
```

On first launch, select your ROM in the launcher. The runtime checks its hash and stores a copy in its config folder.

## AppImage (for releases and the Steam Deck)

```bash
scripts/build-appimage.sh
```

This builds the game in an Ubuntu 22.04 Docker container (`packaging/linux/Dockerfile`), so the binary runs on SteamOS and other distributions with glibc 2.35 or newer, and packages it with linuxdeploy into `dist/SlugfestRecompiled-<version>-x86_64.AppImage`. It needs Docker and the recompiled code from step 2; the ROM isn't packaged. The version comes from `version_string` in `src/main/main.cpp`.

The AppImage starts the game from its own folder, where `assets/` and `recompcontrollerdb.txt` are bundled (`packaging/linux/AppRun`).
