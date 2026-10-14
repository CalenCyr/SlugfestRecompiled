// Uncached (KSEG1, 0xA0000000) RDRAM access.
//
// The game reads and writes its framebuffers and some variables through uncached aliases
// (0xA0xxxxxx), including pointers built at runtime that instruction patches can't find.
// Recompiled code turns an address into `rdram + (addr - 0x80000000)`, so a KSEG1 address lands
// 0x20000000 past the start of RDRAM, which N64ModernRuntime reserves but leaves inaccessible.
//
// On real hardware KSEG0 and KSEG1 are two views of the same memory, so do the same here: back the
// first 8MB of RDRAM with a shared memory file and map it at both rdram and rdram + 0x20000000.
// This runs from the game's on_init_callback, after the runtime's initial ROM copy and before any
// game code.
//
// Past the first 8MB, KSEG1 gets plain zero-filled memory, matching the runtime's 512MB KSEG0 view.
// The game sometimes reads beyond the end of RDRAM through KSEG1 (e.g. the attract-mode screen grab
// in func_80184CD0_gamebase uses the hi-res frame size on a 320x240 framebuffer at the top of
// RDRAM), which real hardware tolerates.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "recomp.h"
#include "librecomp/addresses.hpp"
#include "ultramodern/error_handling.hpp"

#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
    constexpr size_t mirrored_size = 8 * 1024 * 1024;
    constexpr size_t kseg1_offset = 0x20000000;
}

void slugfest_mirror_kseg1(uint8_t* rdram) {
#if defined(__linux__)
    int fd = memfd_create("slugfest_rdram", 0);
    if (fd < 0 || ftruncate(fd, mirrored_size) != 0) {
        perror("[Slugfest] memfd_create/ftruncate");
        ultramodern::error_handling::message_box("Failed to set up uncached RDRAM access.");
        ULTRAMODERN_QUICK_EXIT();
    }

    // Preserve what the runtime has already loaded (the boot segment).
    if (pwrite(fd, rdram, mirrored_size, 0) != (ssize_t)mirrored_size) {
        perror("[Slugfest] pwrite");
        ULTRAMODERN_QUICK_EXIT();
    }

    // Both ranges are inside the runtime's 4GB reservation, so MAP_FIXED only replaces its own pages.
    void* cached = mmap(rdram, mirrored_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    void* uncached = mmap(rdram + kseg1_offset, mirrored_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    close(fd);

    void* uncached_rest = mmap(rdram + kseg1_offset + mirrored_size, recomp::mem_size - mirrored_size,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0);

    if (cached != rdram || uncached != rdram + kseg1_offset || uncached_rest != rdram + kseg1_offset + mirrored_size) {
        perror("[Slugfest] mmap");
        ultramodern::error_handling::message_box("Failed to set up uncached RDRAM access.");
        ULTRAMODERN_QUICK_EXIT();
    }
#else
    // TODO: Windows equivalent (CreateFileMapping + MapViewOfFile3 over the reserved region).
    (void)rdram;
    fprintf(stderr, "[Slugfest] Uncached RDRAM mirror not implemented on this platform\n");
#endif
}

// Physical-style addresses (0x00xxxxxx) used as pointers turn into rdram - 0x80000000 + addr, i.e.
// up to 2GB below RDRAM, where host allocations (e.g. malloc arenas) may live. Reserve whatever
// isn't already mapped there as inaccessible, so such an access faults in the offending function
// instead of silently corrupting host memory.
static void slugfest_guard_below_rdram(uint8_t* rdram) {
#if defined(__linux__)
    constexpr size_t chunk = 16 * 1024 * 1024;
    constexpr size_t guard_size = 0x80000000ULL;
    size_t reserved = 0;
    for (size_t offset = chunk; offset <= guard_size; offset += chunk) {
        void* addr = rdram - offset;
        void* ret = mmap(addr, chunk, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
        if (ret == addr) {
            reserved += chunk;
        }
        else if (ret != MAP_FAILED) {
            munmap(ret, chunk);
        }
    }
    fprintf(stderr, "[Slugfest] Guarded %zu MB below RDRAM\n", reserved / (1024 * 1024));
#else
    (void)rdram;
#endif
}

void slugfest_on_init(uint8_t* rdram, recomp_context* ctx) {
    (void)ctx;
    slugfest_mirror_kseg1(rdram);
    slugfest_guard_below_rdram(rdram);
}
