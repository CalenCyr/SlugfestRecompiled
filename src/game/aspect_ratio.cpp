// Widescreen only where the game can show it.
//
// With RT64's Expand aspect ratio the 3D scenes render correctly at any width, but the menus and
// the title screen don't: the menu panels are drawn with the same full-screen perspective
// projection as gameplay, so RT64 widens them too and shows the parts the 4:3 screen edge used to
// hide, and the title and art screens are backgrounds the CPU writes into the framebuffer.
//
// So while the player has chosen Expand, the renderer uses Original whenever no 3D module is
// loaded. slugfest_update_aspect_ratio runs once per frame (from the frame cap hook).
//
// The game also culls models against its own 4:3 view frustum, so with Expand they popped in and
// out near the sides of the screen. Camera setup (func_8012780C) stores the sine and cosine of the
// horizontal half field of view at camera+0x64/+0x6C, and func_801279E8 builds the left and right
// culling planes from them (the projection uses the cotangent at +0x5C). Hooks around that call
// widen the angle to the window's aspect ratio and then restore it.

#include <atomic>
#include <cmath>
#include <cstring>

#include "recomp.h"

#include "slugfest.h"
#include "recompui/config.h"
#include "ultramodern/config.hpp"

namespace {
    using ultramodern::renderer::AspectRatio;

    // The player's setting, as last loaded or applied in the settings menu.
    std::atomic<AspectRatio> player_choice = AspectRatio::Original;

    // The aspect ratio the renderer currently uses (game thread only).
    bool expanded = false;

    constexpr float original_aspect_ratio = 4.0f / 3.0f;
    constexpr int32_t camera_half_fov_sin = 0x64;
    constexpr int32_t camera_half_fov_cos = 0x6C;
    float saved_sin;
    float saved_cos;

    float read_float(uint8_t* rdram, gpr address) {
        uint32_t word = MEM_W(0, address);
        float value;
        std::memcpy(&value, &word, sizeof(value));
        return value;
    }

    void write_float(uint8_t* rdram, gpr address, float value) {
        uint32_t word;
        std::memcpy(&word, &value, sizeof(word));
        MEM_W(0, address) = word;
    }
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

    expanded = (wanted == AspectRatio::Expand);

    ultramodern::renderer::GraphicsConfig config = ultramodern::renderer::get_graphics_config();
    if (config.ar_option != wanted) {
        config.ar_option = wanted;
        ultramodern::renderer::set_graphics_config(config);
    }
}

// Before func_8012780C calls func_801279E8 ($a0 = camera).
extern "C" void slugfest_widen_culling(uint8_t* rdram, recomp_context* ctx) {
    gpr camera = ctx->r4;
    saved_sin = read_float(rdram, camera + camera_half_fov_sin);
    saved_cos = read_float(rdram, camera + camera_half_fov_cos);

    float widen = expanded ? slugfest::window_aspect_ratio() / original_aspect_ratio : 1.0f;
    if (widen <= 1.0f || saved_cos <= 0.0f) {
        return;
    }

    float tan_half_fov = (saved_sin / saved_cos) * widen;
    float cos_half_fov = 1.0f / std::sqrt(1.0f + tan_half_fov * tan_half_fov);
    write_float(rdram, camera + camera_half_fov_sin, tan_half_fov * cos_half_fov);
    write_float(rdram, camera + camera_half_fov_cos, cos_half_fov);
}

// After func_801279E8 returns ($s0 = camera).
extern "C" void slugfest_restore_culling(uint8_t* rdram, recomp_context* ctx) {
    gpr camera = ctx->r16;
    write_float(rdram, camera + camera_half_fov_sin, saved_sin);
    write_float(rdram, camera + camera_half_fov_cos, saved_cos);
}
