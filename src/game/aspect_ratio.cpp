// Widescreen only where the game can show it.
//
// With RT64's Expand aspect ratio the 3D scenes render correctly at any width, but the menus and
// the title screen don't: the menu panels are drawn with the same full-screen perspective
// projection as gameplay, so RT64 widens them too and shows the parts the 4:3 screen edge used to
// hide, and the title and art screens are backgrounds the CPU writes into the framebuffer.
//
// So while the player has chosen Expand, the renderer uses Original whenever no 3D module is
// loaded. slugfest_update_aspect_ratio runs once per frame (from the frame cap hook).

#include <atomic>

#include "slugfest.h"
#include "recompui/config.h"
#include "ultramodern/config.hpp"

namespace {
    using ultramodern::renderer::AspectRatio;

    // The player's setting, as last loaded or applied in the settings menu.
    std::atomic<AspectRatio> player_choice = AspectRatio::Original;
}

void slugfest::track_aspect_ratio_setting(recomp::config::Config& graphics_config) {
    graphics_config.add_option_change_callback(recompui::config::graphics::options::ar_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            // Temporary changes are previews that haven't been applied yet.
            if (context != recomp::config::OptionChangeContext::Temporary) {
                player_choice = static_cast<AspectRatio>(std::get<uint32_t>(cur_value));
            }
        });
}

void slugfest::set_aspect_ratio_setting(uint32_t value) {
    player_choice = static_cast<AspectRatio>(value);
}

extern "C" void slugfest_update_aspect_ratio() {
    AspectRatio wanted = player_choice;
    if (wanted == AspectRatio::Expand && !slugfest::widescreen_scene_loaded()) {
        wanted = AspectRatio::Original;
    }

    ultramodern::renderer::GraphicsConfig config = ultramodern::renderer::get_graphics_config();
    if (config.ar_option != wanted) {
        config.ar_option = wanted;
        ultramodern::renderer::set_graphics_config(config);
    }
}
