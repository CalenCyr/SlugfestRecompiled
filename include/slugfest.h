#ifndef __SLUGFEST_H__
#define __SLUGFEST_H__

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace slugfest {
    inline const std::u8string program_id = u8"SlugfestRecompiled";
    inline const std::string program_name = "Ken Griffey Jr.'s Slugfest: Recompiled";

    // Rebuilds the working ROM N64Recomp was run against (see N64Recomp/tools/build_decomp_rom.py):
    // the retail ROM followed by every code module, decompressed, starting at 0x1000000.
    std::vector<uint8_t> decompress_rom(std::span<const uint8_t> compressed_rom);

    void register_overlays();
    void init_config();
}

#endif
