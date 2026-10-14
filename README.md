# Ken Griffey Jr.'s Slugfest: Recompiled

A native PC port of Ken Griffey Jr.'s Slugfest (N64, USA), built by static recompilation with [N64Recomp](https://github.com/N64Recomp/N64Recomp), [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime), [RT64](https://github.com/rt64/rt64) and [RecompFrontend](https://github.com/N64Recomp/RecompFrontend). You need your own ROM; none is included.

Status: playable. The intro, menus, create-a-player, attract mode and Exhibition games run with audio, controller and FlashRAM saves. Other modes (Home Run Derby, season) are still being tested, and only Linux is supported so far. See [docs/STATUS.md](docs/STATUS.md) for open issues.

See [BUILDING.md](BUILDING.md) to build it.

## How the game is put together

- The cartridge boots a small loader at `0x80061600` that holds the OS core of libultra (threads, messages, exceptions). It decompresses the main game module, `toplevel`, and jumps to it.
- The ROM has a table of 316 files (`0x80066EB8`) compressed with a block-based MIO0 variant, and a table of 10 code modules (`0x80067DA8`): `toplevel`, `globals` and `arts` at fixed addresses, then overlays sharing two slots (`frontend`/`gamebase` at `0x8014C3C0`; `flythru`/`intro`/`game`/`derby`/`party` at `0x801B0930`).
- Recompilation runs against a working ROM with every module decompressed and appended at `0x1000000`. `src/game/rom_decompression.cpp` rebuilds the same layout from your ROM at runtime, and `src/game/overlays.cpp` tells the runtime which module is loaded, since the game loads them itself.
- Graphics use F3DEX2 and F3DLX2.Rej 2.06 (handled by RT64); the audio microcode is recompiled with RSPRecomp.
- RT64 comes from a [fork](https://github.com/CalenCyr/rt64/tree/slugfest-prim-depth) with one fix (primitive depth, for the player shadows) on top of upstream.
- The game steps its logic once per frame with no frame limiter, so `src/game/frame_pacing.cpp` holds it at its original 30 fps.

## Credits

The app skeleton (`src/main/main.cpp`, `CMakeLists.txt`) and the UI assets are adapted from [BanjoRecomp](https://github.com/BanjoRecomp/BanjoRecomp) (GPL-3.0). Fonts: Inter and Noto Emoji (SIL Open Font License), [PromptFont](https://shinmera.com/promptfont) (see `assets/promptfont/LICENSE.txt`).

## License

GPL-3.0. See [LICENSE](LICENSE).
