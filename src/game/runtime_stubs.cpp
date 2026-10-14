// libultra functions the recompiled code calls that this N64ModernRuntime version doesn't provide.

#include <cmath>
#include <cstdio>
#include <cstdint>

#include "recomp.h"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"

namespace {
    constexpr uint32_t k1_to_phys(uint32_t addr) {
        return addr & 0x1FFFFFFF;
    }

    // osPiReadIo / osPiRawReadIo (devAddr, *data): arts reads ROM headers this way.
    void rom_read_io(uint8_t* rdram, recomp_context* ctx) {
        uint32_t physical_addr = k1_to_phys(recomp::rom_base | (uint32_t)ctx->r4);
        gpr dram_addr = ctx->r5;

        if (physical_addr >= recomp::rom_base) {
            recomp::do_rom_pio(rdram, dram_addr, physical_addr);
        }
        else {
            MEM_W(0, dram_addr) = 0;
        }
        ctx->r2 = 0;
    }
}

// The boot loader's osPiRawStartDma(direction, devAddr, dramAddr, size), renamed in symbol_addrs.txt
// because N64ModernRuntime only provides a fatal stub for osPiRawStartDma. It loads toplevel and
// then polls osPiGetStatus, which the runtime reports as idle, so a synchronous read is enough.
extern "C" void boot_osPiRawStartDma(uint8_t* rdram, recomp_context* ctx) {
    uint32_t direction = (uint32_t)ctx->r4;
    uint32_t physical_addr = k1_to_phys(recomp::rom_base | (uint32_t)ctx->r5);
    gpr dram_addr = ctx->r6;
    uint32_t size = (uint32_t)ctx->r7;

    if (direction != 0 /* OS_READ */ || physical_addr < recomp::rom_base) {
        fprintf(stderr, "[Slugfest] Unsupported boot DMA: direction %u, address 0x%08X\n", direction, physical_addr);
        ctx->r2 = -1;
        return;
    }

    recomp::do_rom_read(rdram, dram_addr, physical_addr, size);
    ctx->r2 = 0;
}

extern "C" void osPiReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    rom_read_io(rdram, ctx);
}

extern "C" void osPiRawReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    rom_read_io(rdram, ctx);
}

// Only reached from the FlashRAM driver, which the runtime reimplements.
extern "C" void osEPiWriteIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    ctx->r2 = 0;
}

// Crash/debug screen support in toplevel; there's no faulted thread to report.
extern "C" void osGetCurrFaultedThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    ctx->r2 = 0;
}

extern "C" void __osSetWatchLo_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

// RSP status/register access from toplevel's RSP debug screens and its task-hang recovery.
// RSP tasks run synchronously on the host, so report a halted RSP and ignore writes.
extern "C" void __osSpGetStatus_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    ctx->r2 = 0x1; // SP_STATUS_HALT
}

extern "C" void __osSpSetStatus_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    (void)ctx;
}

extern "C" void __osSpRawReadIo_recomp(uint8_t* rdram, recomp_context* ctx) {
    MEM_W(0, ctx->r5) = 0;
    ctx->r2 = 0;
}

// u64 __f_to_ull(f32): float in $f12, 64-bit result in $v0 (high) / $v1 (low).
extern "C" void __f_to_ull_recomp(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    float value = ctx->f12.fl;
    uint64_t result = value <= 0.0f ? 0 : (uint64_t)value;
    ctx->r2 = (int32_t)(result >> 32);
    ctx->r3 = (int32_t)(result & 0xFFFFFFFF);
}
