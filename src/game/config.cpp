#include <filesystem>

#include "slugfest.h"
#include "recompui/config.h"
#include "util/file.h"
#include "ultramodern/config.hpp"

void slugfest::init_config() {
    std::filesystem::path recomp_dir = recompui::file::get_app_folder_path();

    if (!recomp_dir.empty()) {
        std::filesystem::create_directories(recomp_dir);
    }

    bool first_run = !std::filesystem::exists(recomp_dir / "graphics.json");

    recompui::config::GeneralTabOptions general_options{};
    general_options.has_rumble_strength = true;
    general_options.has_gyro_sensitivity = false;
    general_options.has_mouse_sensitivity = false;
    recompui::config::create_general_tab(general_options);

    slugfest::track_scene_settings(recompui::config::create_graphics_tab());
    recompui::config::create_controls_tab();
    recompui::config::create_sound_tab();
    recompui::config::create_mods_tab();

    recompui::config::finalize();

    // Look the graphics config up again: the reference create_graphics_tab returned doesn't survive
    // the other tabs being created.
    auto& graphics_config = recompui::config::get_graphics_config();

    // The Display refresh rate makes RT64 generate in-between frames, which this game (30 fps, no
    // interpolation support) shows as motion at double speed. Default new installs to Original.
    if (first_run) {
        graphics_config.set_option_value(recompui::config::graphics::options::rr_option,
            static_cast<uint32_t>(ultramodern::renderer::RefreshRate::Original));
    }

    slugfest::set_scene_settings(
        std::get<uint32_t>(graphics_config.get_option_value(recompui::config::graphics::options::ar_option)),
        std::get<uint32_t>(graphics_config.get_option_value(recompui::config::graphics::options::rr_option)));
}
