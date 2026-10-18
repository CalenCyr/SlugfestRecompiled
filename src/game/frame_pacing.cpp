// Frame rate cap.
//
// The game advances its logic and animations by a fixed step per frame and has no frame rate limit of
// its own: on hardware, rendering the hi-res frames kept it at 30 frames per second or below.
// Recompiled code and RT64 can finish a frame every video refresh, so the game ran at up to 60 and
// played too fast (the menus always, gameplay depending on RT64's presentation mode).
//
// A hook at the entry of the game's frame submit (func_8012D2E4, see kengriffey.toml) waits until
// enough video refreshes have passed since the previous frame. The game's own refresh counter
// (0x80090674) is used, so the frame time the game measures (0x8014A45C) matches the capped rate.
//
// SLUGFEST_FRAME_REFRESHES overrides the refreshes per frame (1 disables the cap).

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "recomp.h"

extern "C" void slugfest_yield_1ms(uint8_t* rdram);
extern "C" void slugfest_update_scene_settings();

namespace {
    constexpr gpr vi_counter_addr = (gpr)(int32_t)0x80090674;
    constexpr uint32_t default_refreshes_per_frame = 2;

    uint32_t refreshes_per_frame() {
        static uint32_t value = [] {
            uint32_t refreshes = default_refreshes_per_frame;
            if (const char* env = std::getenv("SLUGFEST_FRAME_REFRESHES")) {
                long parsed = std::strtol(env, nullptr, 10);
                if (parsed >= 1 && parsed <= 6) {
                    refreshes = (uint32_t)parsed;
                }
            }
            fprintf(stderr, "[Slugfest] Running at one frame per %u refreshes\n", refreshes);
            return refreshes;
        }();
        return value;
    }
}

extern "C" void slugfest_pace_frame(uint8_t* rdram, recomp_context* ctx) {
    (void)ctx;
    static bool have_previous = false;
    static uint32_t previous_frame = 0;

    uint32_t refreshes = refreshes_per_frame();
    if (have_previous) {
        while ((uint32_t)MEM_W(0, vi_counter_addr) - previous_frame < refreshes) {
            slugfest_yield_1ms(rdram);
        }
    }

    previous_frame = (uint32_t)MEM_W(0, vi_counter_addr);
    have_previous = true;

    slugfest_update_scene_settings();
}
