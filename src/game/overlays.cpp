// Tells N64ModernRuntime which code module is in memory.
//
// The game DMAs its code modules from compressed ROM files into a scratch buffer and decompresses
// them itself, so the runtime never sees a DMA from a ROM range it can match to a recompiled
// section. Hooks added in kengriffey.toml call into this file instead:
//   - slugfest_on_toplevel_loaded: boot main, just before it jumps into toplevel.
//   - slugfest_on_load_code: entry of toplevel's load_code(file_index, dest) (func_80071708).

#include <cstdio>
#include <iterator>

#include "recomp.h"
#include "librecomp/overlays.hpp"

namespace {
    struct Module {
        const char* name;
        uint32_t code_file; // index in the boot segment's file table
        uint32_t rom;       // offset in the working ROM (see build_decomp_rom.py)
        uint32_t vram;
        uint32_t size;      // code + data, matching the recompiled section size
        bool loaded;
    };

    // Matches the module table at 0x80067DA8 and the segments in kengriffeyslugfest.yaml.
    Module modules[] = {
        { "toplevel", 0,  0x1000000, 0x8006A8A0, 0x2E9F0, false },
        { "globals",  1,  0x102E9F0, 0x800F9B30, 0x0D0F0, false },
        { "arts",     3,  0x103BAE0, 0x8010A880, 0x413C0, false },
        { "frontend", 5,  0x107CEA0, 0x8014C3C0, 0xACED0, false },
        { "gamebase", 7,  0x1129D70, 0x8014C3C0, 0x609F0, false },
        { "flythru",  9,  0x118A760, 0x801B0930, 0x01560, false },
        { "intro",    11, 0x118BCC0, 0x801B0930, 0x016D0, false },
        { "game",     13, 0x118D390, 0x801B0930, 0x7BE90, false },
        { "derby",    15, 0x1209220, 0x801B0930, 0x074C0, false },
        { "party",    17, 0x12106E0, 0x801B0930, 0x0A710, false },
    };

    void load_module(Module& module, uint32_t dest) {
        if (dest != module.vram) {
            fprintf(stderr, "[Slugfest] %s loaded at 0x%08X instead of 0x%08X\n", module.name, dest, module.vram);
        }

        // unload_overlays() aborts on a partial overlap (e.g. frontend replacing gamebase + game),
        // so unload every module that overlaps the new one over its own full range.
        for (Module& other : modules) {
            bool overlaps = other.loaded && other.vram < dest + module.size && dest < other.vram + other.size;
            if (overlaps) {
                unload_overlays((int32_t)other.vram, other.size);
                other.loaded = false;
            }
        }

        load_overlays(module.rom, (int32_t)dest, module.size);
        module.loaded = true;
    }
}

extern "C" void slugfest_on_toplevel_loaded(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
    load_module(modules[0], modules[0].vram);
}

extern "C" void slugfest_on_load_code(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    uint32_t file_index = (uint32_t)ctx->r4;
    uint32_t dest = (uint32_t)ctx->r5;

    // load_code is also used for each module's data file, which has no code to register.
    for (Module& module : modules) {
        if (module.code_file == file_index && file_index != 0) {
            load_module(module, dest);
            return;
        }
    }
}
