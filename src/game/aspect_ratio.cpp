// Widescreen only where the game can show it.
//
// With RT64's Expand aspect ratio the 3D scenes render correctly at any width, but the menus and
// the title screen don't: the menu panels are drawn with the same full-screen perspective
// projection as gameplay, so RT64 widens them too and shows the parts the 4:3 screen edge used to
// hide, and the title and art screens are backgrounds the CPU writes into the framebuffer.
//
// So while the player has chosen Expand, the renderer uses Original whenever no 3D module is
// loaded. slugfest_update_scene_settings runs once per frame (from the frame cap hook).
//
// On those screens RT64 also uploads the whole framebuffer when the CPU rewrites it (see
// setUploadWholeFramebuffers in the RT64 fork). The title copies its background in every frame
// and draws the fading PRESS START over it; with only the changed pixels uploaded, the faint edges
// of the letters were never reset at higher resolutions and built up into a bright fringe. The
// same uploads fixed the boot logos showing as a grid of dots in some in-between frames with the
// Display refresh rate.
//
// Only the aspect ratio is switched. Switching the refresh rate too (Original on these screens,
// Display in 3D scenes) made RT64's graphics thread run away on a display list after entering the
// stadium fly-through a second time, freezing the game while its memory grew until the system ran
// out (2026-10-18).
//
// The game also culls models against its own 4:3 view frustum, so with Expand they popped in and
// out near the sides of the screen. Camera setup (func_8012780C) stores the sine and cosine of the
// horizontal half field of view at camera+0x64/+0x6C, and func_801279E8 builds the left and right
// culling planes from them (the projection uses the cotangent at +0x5C). Hooks around that call
// widen the angle to the window's aspect ratio, plus a margin, and then restore it.
//
// The HUD would stay in the centered 4:3 area: RT64 moves 2D elements to the screen edges only when
// the display list says which edge they belong to, which this game doesn't. While widescreen is in
// use, RT64's HUD auto-anchoring (rt64_rect_anchor.cpp in the RT64 fork) does it instead, by as much
// as the HUD Placement setting allows.

#include <atomic>
#include <cmath>
#include <cstring>

#include "recomp.h"

#include "slugfest.h"
#include "recompui/config.h"
#include "ultramodern/config.hpp"

namespace RT64 {
    // lib/rt64/src/hle/rt64_framebuffer_manager.h
    void setUploadWholeFramebuffers(bool enabled);
    // lib/rt64/src/hle/rt64_rect_anchor.h
    void setRectAutoAnchor(bool enabled);
}

namespace {
    using ultramodern::renderer::AspectRatio;

    // The player's setting, as last loaded or applied in the settings menu.
    std::atomic<AspectRatio> player_aspect_ratio = AspectRatio::Original;

    // The aspect ratio the renderer currently uses (game thread only).
    bool expanded = false;

    constexpr float original_aspect_ratio = 4.0f / 3.0f;

    // Extra width for the widened culling planes. Objects are tested by a bounding volume around
    // their position, so with planes exactly at the screen edges, models at the sides appeared a
    // little after they were already partly on screen.
    constexpr float culling_margin = 1.2f;
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

void slugfest::track_scene_settings(recomp::config::Config& graphics_config) {
    graphics_config.add_option_change_callback(recompui::config::graphics::options::ar_option,
        [](recomp::config::ConfigValueVariant cur_value, recomp::config::ConfigValueVariant, recomp::config::OptionChangeContext context) {
            // Temporary changes are previews that haven't been applied yet.
            if (context != recomp::config::OptionChangeContext::Temporary) {
                player_aspect_ratio = static_cast<AspectRatio>(std::get<uint32_t>(cur_value));
            }
        });
}

void slugfest::set_scene_settings(uint32_t aspect_ratio) {
    player_aspect_ratio = static_cast<AspectRatio>(aspect_ratio);
}

extern "C" void slugfest_update_scene_settings() {
    bool scene_3d = slugfest::widescreen_scene_loaded();
    AspectRatio aspect_ratio = scene_3d ? player_aspect_ratio.load() : AspectRatio::Original;

    expanded = (aspect_ratio == AspectRatio::Expand);
    RT64::setUploadWholeFramebuffers(!scene_3d);
    RT64::setRectAutoAnchor(expanded);

    ultramodern::renderer::GraphicsConfig config = ultramodern::renderer::get_graphics_config();
    if (config.ar_option != aspect_ratio) {
        config.ar_option = aspect_ratio;
        ultramodern::renderer::set_graphics_config(config);
    }
}

// Before func_8012780C calls func_801279E8 ($a0 = camera).
extern "C" void slugfest_widen_culling(uint8_t* rdram, recomp_context* ctx) {
    gpr camera = ctx->r4;
    saved_sin = read_float(rdram, camera + camera_half_fov_sin);
    saved_cos = read_float(rdram, camera + camera_half_fov_cos);

    float widen = expanded ? (slugfest::window_aspect_ratio() / original_aspect_ratio) * culling_margin : 1.0f;
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
