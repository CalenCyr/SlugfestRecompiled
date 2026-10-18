// N64 Sports logo at boot.
//
// The boot thread (func_8006AEE0) draws the N64 Sports logo into the framebuffer with
// func_8006ACE0 (a 144x109 image read from ROM 0x4B1C10), shows it, and then loads the game's code
// modules. On hardware that loading kept the logo on screen for one to two seconds; recompiled, it
// is replaced almost at once. A hook right after func_8006ACE0 returns keeps it up instead.

#include <chrono>
#include <cstdint>

#include "recomp.h"

extern "C" void slugfest_yield_1ms(uint8_t* rdram);

namespace {
    constexpr auto boot_logo_time = std::chrono::milliseconds(1500);
}

extern "C" void slugfest_hold_boot_logo(uint8_t* rdram, recomp_context* ctx) {
    (void)ctx;
    auto end = std::chrono::steady_clock::now() + boot_logo_time;
    while (std::chrono::steady_clock::now() < end) {
        slugfest_yield_1ms(rdram);
    }
}
