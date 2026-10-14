# Status and handoff notes

Last updated: 2026-10-13. Published at https://github.com/CalenCyr/SlugfestRecompiled (source only; nothing derived from the game is committed).

## Where things stand

- Boots, intro, title screen, menus, attract mode and an Exhibition game all run, with audio, the controller in port 1, and FlashRAM saving (`~/.config/SlugfestRecompiled/saves/`).
- Regenerate code with `scripts/regenerate.sh` (inputs in `recomp/`; N64Recomp/RSPRecomp built from the `tools/N64Recomp` submodule into `build-tools/`). Build with CMake (see BUILDING.md). Run from the project root.
- Defaults for new installs: aspect ratio Original (Expand makes the 2D title/art screens flicker) and refresh rate Original (Display makes RT64 add in-between frames, and motion plays at double speed even though the game runs at the correct 30 fps).

## Open issues (in suggested order)

1. **Expand (widescreen) on 2D screens:** the background the CPU writes into the framebuffer is misplaced. Needs a game-specific RT64 fix; optional.
2. **More play-testing:** Home Run Derby, season mode, save/load, Controller Pak, rumble. Expect more busy-wait loops like `func_8006EA30` (fix: `[[patches.hook]]` calling `yield_self_1ms(rdram)` inside the loop).
3. **Occasional crash when closing the window** (seen once, 2026-10-13, core dump unusable because the binary had been rebuilt). Harmless for saves; if it recurs, `coredumpctl debug` on the matching build.
4. **Windows:** `src/game/memory_mirror.cpp` (KSEG1 mirror + guard below RDRAM) is Linux-only.
5. **Cleanup:** if the memory guard ever fires, the core dump names the game function writing through a physical address. `~/Code/N64Recomp` is no longer used by this project (N64Recomp is now the `tools/N64Recomp` submodule) and can be deleted, along with the old generated files in it.

## Debugging recipes

- Game threads stuck: `gdb -p $(pgrep -x SlugfestRecompi) -batch -ex "thread apply all bt 30"` and look at the `[Game] N` threads (5 = main loop, 4 = graphics scheduler, 3 = coordinator, 1 = idle).
- Crashes: `coredumpctl list`, then `coredumpctl debug <pid> --debugger-arguments="-batch -ex 'bt 25'"`; print `ctx->rN` in a `func_*` frame to see the game registers.
- Temporary tracing: add `[[patches.hook]]` entries in `recomp/kengriffey.toml` that call a native `extern "C"` function (pattern used before: a `debug_hooks.cpp` with `slugfest_debug_sched(rdram, ctx, point)`), regenerate, rebuild, and remove when done.
- RT64 inspector: set `"developer_mode": true` in graphics.json (game closed), F1 in game. Debugger tab → right-click a pixel to list its draw calls. F4 doesn't reach the Pause button (RT64 grabs it); "View Depth Buffer" crashes with MSAA on, so set `"msaa_option": "None"` first.
- Reading game memory live: `/proc/<pid>/maps` has the `memfd:slugfest_rdram` mapping; words are stored native little-endian at `base + (addr & 0x7FFFFF)` (byte fields are XOR-3 swapped).
- Run with a log: `./build/SlugfestRecompiled > run.log 2>&1` (stderr has the runtime and `[Slugfest]` messages).

## Fixes made so far (for context)

Recompiler side (`recomp/`):
- libultra identified in the boot segment, toplevel, arts (PI handle API) and frontend (FlashRAM; pinned with `segment:frontend` because frontend overlaps gamebase).
- `boot_main` (renamed from `main`), `boot_osPiRawStartDma` (renamed and ignored; implemented natively).
- Two merged functions split out in gamebase (`func_80166870_gamebase`, `func_8016B52C_gamebase`).
- Instruction patches: PC-relative `bltzal` table loads in toplevel audio code; KSEG1 `lui 0xA000` in game code; scheduler reply send changed to NOBLOCK (`func_800753A0`).
- Hooks: overlay registration (`boot_main`, `func_80071708`), scheduler waits for the RDP before the next gfx task (`func_8007572C`), yield in the sound-stop busy-wait (`func_8006EA30`), frame cap at the frame submit (`func_8012D2E4`).

RT64 (`lib/rt64` points at the fork https://github.com/CalenCyr/rt64, branch `slugfest-prim-depth`; worth offering upstream):
- Player shadows flickered. The game draws each shadow as the flattened player model with primitive depth 0x7FFF, depth compare and update on, so overlapping triangles blend only once. RT64 scaled prim depth by 1/32767 (0x7FFF = 1.0, equal to the cleared depth buffer, so `LESS` failed except for rounding noise). Fixed by using the RDP's 18-bit scale (`z << 3`, matching `D16::toF`), and by collapsing the viewport depth range to the prim depth for those draws so every pixel gets exactly the same depth.
- To update RT64: rebase `slugfest-prim-depth` on the new upstream commit, push to the fork, and update the submodule pointer.

Runtime project (`src/`):
- `overlays.cpp`: tracks loaded modules and calls `unload_overlays`/`load_overlays` per module.
- `frame_pacing.cpp`: caps the game at one frame per 2 video refreshes (30 fps). The game steps its logic a fixed amount per frame with no limiter of its own; uncapped, the menus ran at 60 and gameplay ran at 30 or 60 depending on RT64's presentation mode (Display = double speed). Uses the game's refresh counter (0x80090674) so its measured frame time (0x8014A45C) stays consistent. `SLUGFEST_FRAME_REFRESHES=N` overrides.
- `runtime_stubs.cpp`: libultra functions this runtime version lacks.
- `memory_mirror.cpp`: KSEG1 mirror of the first 8MB of RDRAM (zero-filled past 8MB, like KSEG0; the attract-mode screen grab reads past the end), and a guard region below RDRAM.
- `main.cpp`: single-player mode reports only port 1 (runtime bug: `__osContGetInitData` assigns instead of ORing the pattern bits); save type AllowAll (game reads flash via raw DMA too).
