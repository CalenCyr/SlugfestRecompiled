// C++ port of N64Recomp/tools/build_decomp_rom.py. The two must produce identical output, since
// the section ROM addresses in RecompiledFuncs (and in src/game/overlays.cpp) come from it.

#include <cassert>
#include <cstring>

#include "slugfest.h"

namespace {
    // Boot segment (RAM 0x80061600 = ROM 0x1000) data, converted to ROM offsets.
    constexpr uint32_t ram_to_rom = 0x80060600;
    constexpr uint32_t file_table_rom = 0x80066EB8 - ram_to_rom;   // {rom_start, rom_end, size} x 316
    constexpr uint32_t module_table_rom = 0x80067DA8 - ram_to_rom; // 0x30-byte entries
    constexpr uint32_t module_count = 10;
    constexpr uint32_t block_size = 0x2000;
    constexpr uint32_t append_start = 0x1000000;

    uint32_t read32(std::span<const uint8_t> data, size_t offset) {
        return (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3];
    }

    uint16_t read16(std::span<const uint8_t> data, size_t offset) {
        return (data[offset] << 8) | data[offset + 1];
    }

    // One ROM block: {u16 unk, u16 out_size, u16 comp_off, u16 uncomp_off} then an MIO0 body.
    // Back-references may reach into earlier blocks' output, so everything appends to `out`.
    void decode_block(std::span<const uint8_t> block, std::vector<uint8_t>& out) {
        uint16_t out_size = read16(block, 2);
        uint16_t comp = read16(block, 4);
        uint16_t uncomp = read16(block, 6);

        if (comp == 0) {
            // Stored block.
            out.insert(out.end(), block.begin() + 8, block.begin() + 8 + out_size);
            return;
        }

        size_t layout = 8;
        size_t end = out.size() + out_size;
        uint32_t bits = 0;
        int num_bits = 0;
        while (out.size() < end) {
            if (num_bits == 0) {
                bits = read32(block, layout);
                layout += 4;
                num_bits = 32;
            }
            if (bits & 0x80000000) {
                out.push_back(block[uncomp++]);
            }
            else {
                uint16_t v = read16(block, comp);
                comp += 2;
                size_t dist = (v & 0xFFF) + 1;
                for (int i = 0; i < (v >> 12) + 3; i++) {
                    out.push_back(out[out.size() - dist]);
                }
            }
            bits <<= 1;
            num_bits--;
        }
    }

    bool append_file(std::span<const uint8_t> rom, uint32_t index, std::vector<uint8_t>& out) {
        size_t entry = file_table_rom + index * 12;
        uint32_t start = read32(rom, entry);
        uint32_t end = read32(rom, entry + 4);
        uint32_t size = read32(rom, entry + 8);
        size_t out_start = out.size();

        if (size < 2) {
            out.insert(out.end(), rom.begin() + start, rom.begin() + end);
            return true;
        }
        for (uint32_t pos = start; pos < end; pos += block_size) {
            decode_block(rom.subspan(pos, std::min(block_size, end - pos)), out);
        }
        return out.size() - out_start == size;
    }
}

std::vector<uint8_t> slugfest::decompress_rom(std::span<const uint8_t> compressed_rom) {
    // The runtime has already checked the hash; this guards against copying the file to another project.
    if (compressed_rom.size() != append_start || std::memcmp(compressed_rom.data() + 0x3B, "NKJE", 4) != 0) {
        assert(false);
        return {};
    }

    std::vector<uint8_t> ret(compressed_rom.begin(), compressed_rom.end());
    ret.reserve(0x1300000);

    for (uint32_t i = 0; i < module_count; i++) {
        size_t entry = module_table_rom + i * 0x30 + 0x10;
        uint32_t code_file = read32(compressed_rom, entry);
        uint32_t data_file = read32(compressed_rom, entry + 4);

        if (!append_file(compressed_rom, code_file, ret)) {
            assert(false);
            return {};
        }
        // toplevel (module 0) is a single file containing both code and data.
        if (i != 0 && !append_file(compressed_rom, data_file, ret)) {
            assert(false);
            return {};
        }
    }

    return ret;
}
