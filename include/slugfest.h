#ifndef __SLUGFEST_H__
#define __SLUGFEST_H__

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace recomp::config {
    class Config;
}

namespace slugfest {
    inline const std::u8string program_id = u8"SlugfestRecompiled";
    inline const std::string program_name = "Ken Griffey Jr.'s Slugfest: Recompiled";

    // Rebuilds the working ROM N64Recomp was run against (see N64Recomp/tools/build_decomp_rom.py):
    // the retail ROM followed by every code module, decompressed, starting at 0x1000000.
    std::vector<uint8_t> decompress_rom(std::span<const uint8_t> compressed_rom);

    void register_overlays();
    // True while a 3D module (game, derby, party, flythru) is loaded, as opposed to the menus or title.
    bool widescreen_scene_loaded();
    void init_config();

    // Expand only in 3D scenes (see aspect_ratio.cpp).
    void track_aspect_ratio_setting(recomp::config::Config& graphics_config);
    void set_aspect_ratio_setting(uint32_t value);
}

#endif
