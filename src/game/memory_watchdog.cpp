// Memory watchdog.
//
// Twice (2026-10-18) the game ran away while going into gameplay and grew past 8 GB, until the
// kernel's OOM killer stopped it and took other applications with it. Until the cause is found,
// a thread checks the process's resident memory every second and aborts past a limit, so the
// system stays usable and the core dump (coredumpctl) shows what every thread was doing.
//
// SLUGFEST_MEMORY_LIMIT_MB overrides the limit (0 disables the watchdog). Linux only.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "slugfest.h"

namespace {
    constexpr long default_limit_mb = 3072;

    long resident_mb() {
        long pages = 0;
        long resident = 0;
        FILE* f = std::fopen("/proc/self/statm", "r");
        if (f == nullptr) {
            return 0;
        }
        if (std::fscanf(f, "%ld %ld", &pages, &resident) != 2) {
            resident = 0;
        }
        std::fclose(f);
        return resident * 4096 / (1024 * 1024);
    }
}

void slugfest::start_memory_watchdog() {
#ifdef __linux__
    long limit_mb = default_limit_mb;
    if (const char* env = std::getenv("SLUGFEST_MEMORY_LIMIT_MB")) {
        limit_mb = std::strtol(env, nullptr, 10);
    }
    if (limit_mb <= 0) {
        return;
    }

    std::thread([limit_mb]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            long used = resident_mb();
            if (used > limit_mb) {
                std::fprintf(stderr, "[Slugfest] Memory use reached %ld MB (limit %ld MB), aborting\n", used, limit_mb);
                std::abort();
            }
        }
    }).detach();
#endif
}
