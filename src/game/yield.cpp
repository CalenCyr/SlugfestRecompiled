// Yield for native busy-wait loops (the frame cap and the hooks in kengriffey.toml).
//
// On quit, recomp::start unmaps RDRAM without stopping the game threads. Threads blocked in the
// runtime never touch it again, but a loop that keeps yielding does: yield_self_1ms reads the
// running queue from RDRAM, which crashed when the window was closed while the frame cap waited.
// Once the runtime is exiting, park the thread instead; the process ends without it.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#include "ultramodern/ultramodern.hpp"

extern std::atomic_bool exited;

namespace {
    [[noreturn]] void park_forever() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::hours(1));
        }
    }
}

extern "C" void slugfest_yield_1ms(uint8_t* rdram) {
    if (exited) {
        park_forever();
    }
    ultramodern::wait_for_external_message_timed(rdram, 1);
    if (exited) {
        park_forever();
    }
    ultramodern::check_running_queue(rdram);
}
