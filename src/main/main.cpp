// Application entry point. Adapted from BanjoRecomp's src/main/main.cpp (GPL-3.0), with the
// Banjo-specific launcher, theme, patch and mod-API pieces removed.

#include <array>
#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "nfd.h"

#include "ultramodern/ultra64.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"
#define SDL_MAIN_HANDLED
#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"
#undef None
#undef Status
#undef LockMask
#undef ControlMask
#undef Success
#undef Always
#endif

#include "recompui/recompui.h"
#include "recompui/program_config.h"
#include "recompui/renderer.h"
#include "recompui/config.h"
#include "core/ui_context.h"
#include "util/file.h"
#include "recompinput/input_events.h"
#include "recompinput/recompinput.h"
#include "recompinput/profiles.h"
#include "librecomp/game.hpp"
#include "librecomp/mods.hpp"
#include "librecomp/rsp.hpp"

#include "slugfest.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <timeapi.h>
#include "SDL_syswm.h"
#endif

#include "../../lib/rt64/src/contrib/stb/stb_image.h"
#include "icon_bytes.h"

const std::string version_string = "0.1.0";

template<typename... Ts>
void exit_error(const char* str, Ts ...args) {
    ((void)fprintf(stderr, str, args), ...);
    assert(false);
    ultramodern::error_handling::quick_exit(__FILE__, __LINE__, __FUNCTION__);
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC) > 0) {
        exit_error("Failed to initialize SDL2: %s\n", SDL_GetError());
    }

    fprintf(stdout, "SDL Video Driver: %s\n", SDL_GetCurrentVideoDriver());

    return {};
}

// The game supports a Controller Pak and a Rumble Pak; report a controller with a Rumble Pak
// for each player that has a device assigned.
// In single-player mode only port 1 is reported: N64ModernRuntime's __osContGetInitData assigns
// (rather than ORs) each connected port's bit into osContInit's pattern, so reporting all four
// ports leaves only port 4's bit set and the game sees no controller in port 1.
ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    bool connected = recompinput::players::is_single_player_mode()
        ? controller_num == 0
        : recompinput::players::get_player_is_assigned(controller_num);

    if (connected) {
        return ultramodern::input::connected_device_info_t{
            .connected_device = ultramodern::input::Device::Controller,
            .connected_pak = ultramodern::input::Pak::RumblePak,
        };
    }

    return ultramodern::input::connected_device_info_t{
        .connected_device = ultramodern::input::Device::None,
        .connected_pak = ultramodern::input::Pak::None,
    };
}

#if defined(__gnu_linux__)
static bool set_window_icon(SDL_Window* window) {
    int width, height, bytes_per_pixel;
    void* data = stbi_load_from_memory(reinterpret_cast<const uint8_t*>(icon_bytes), sizeof(icon_bytes), &width, &height, &bytes_per_pixel, 4);
    if (data == nullptr) {
        return false;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(data, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        stbi_image_free(data);
        return false;
    }

    SDL_SetWindowIcon(window, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(data);
    return true;
}
#endif

SDL_Window* window;

float slugfest::window_aspect_ratio() {
    int width = 0;
    int height = 0;
    if (window != nullptr) {
        SDL_GetWindowSizeInPixels(window, &width, &height);
    }
    return (width > 0 && height > 0) ? float(width) / float(height) : 4.0f / 3.0f;
}

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    uint32_t flags = SDL_WINDOW_RESIZABLE;

#if defined(__APPLE__)
    flags |= SDL_WINDOW_METAL;
#elif defined(RT64_SDL_WINDOW_VULKAN)
    flags |= SDL_WINDOW_VULKAN;
#endif

    window = SDL_CreateWindow(slugfest::program_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, flags);

    if (window == nullptr) {
        exit_error("Failed to create window: %s\n", SDL_GetError());
    }

    SDL_SysWMinfo wm_info;
    SDL_VERSION(&wm_info.version);
    SDL_GetWindowWMInfo(window, &wm_info);

#if defined(__gnu_linux__)
    set_window_icon(window);
#endif

#if defined(_WIN32)
    return ultramodern::renderer::WindowHandle{ wm_info.info.win.window, GetCurrentThreadId() };
#elif defined(__linux__) || defined(__ANDROID__)
    return ultramodern::renderer::WindowHandle{ window };
#elif defined(__APPLE__)
    SDL_MetalView view = SDL_Metal_CreateView(window);
    return ultramodern::renderer::WindowHandle{ wm_info.info.cocoa.window, SDL_Metal_GetLayer(view) };
#else
    static_assert(false && "Unimplemented");
#endif
}

void update_gfx(void*) {
    recompinput::handle_events();
}

// Audio output: the game's audio thread hands over interleaved 16-bit stereo, which is resampled
// to the output device's rate with SDL's converter.
static SDL_AudioCVT audio_convert;
static SDL_AudioDeviceID audio_device = 0;

static uint32_t sample_rate = 48000;
static uint32_t output_sample_rate = 48000;
constexpr uint32_t input_channels = 2;
static uint32_t output_channels = 2;

// Number of samples to duplicate at the end of each buffer to avoid gaps when resampling.
constexpr uint32_t duplicated_input_frames = 4;
static uint32_t discarded_output_frames;

constexpr uint32_t bytes_per_frame = input_channels * sizeof(float);

void queue_samples(int16_t* audio_data, size_t sample_count) {
    static std::vector<float> swap_buffer;
    static std::array<float, duplicated_input_frames * input_channels> duplicated_sample_buffer;

    size_t resampled_sample_count = sample_count + duplicated_input_frames * input_channels;
    size_t max_sample_count = std::max(resampled_sample_count, resampled_sample_count * audio_convert.len_mult);
    if (max_sample_count > swap_buffer.size()) {
        swap_buffer.resize(max_sample_count);
    }

    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        swap_buffer[i] = duplicated_sample_buffer[i];
    }

    float cur_main_volume = static_cast<float>(recompui::config::sound::get_main_volume()) / 100.0f;
    // The N64 audio buffer has its channels swapped relative to the host.
    for (size_t i = 0; i < sample_count; i += input_channels) {
        swap_buffer[i + 0 + duplicated_input_frames * input_channels] = audio_data[i + 1] * (0.5f / 32768.0f) * cur_main_volume;
        swap_buffer[i + 1 + duplicated_input_frames * input_channels] = audio_data[i + 0] * (0.5f / 32768.0f) * cur_main_volume;
    }

    assert(sample_count > duplicated_input_frames * input_channels);

    for (size_t i = 0; i < duplicated_input_frames * input_channels; i++) {
        duplicated_sample_buffer[i] = swap_buffer[i + sample_count];
    }

    audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    audio_convert.len = (sample_count + duplicated_input_frames * input_channels) * sizeof(swap_buffer[0]);

    if (SDL_ConvertAudio(&audio_convert) < 0) {
        printf("Error using SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error using SDL audio converter");
    }

    uint64_t cur_queued_microseconds = uint64_t(SDL_GetQueuedAudioSize(audio_device)) / bytes_per_frame * 1000000 / sample_rate;
    uint32_t num_bytes_to_queue = audio_convert.len_cvt - output_channels * discarded_output_frames * sizeof(swap_buffer[0]);
    float* samples_to_queue = swap_buffer.data() + output_channels * discarded_output_frames / 2;

    // Drop samples if too much audio is queued, to keep latency bounded.
    uint32_t skip_factor = cur_queued_microseconds / 100000;
    if (skip_factor != 0) {
        uint32_t skip_ratio = 1 << skip_factor;
        num_bytes_to_queue /= skip_ratio;
        for (size_t i = 0; i < num_bytes_to_queue / (output_channels * sizeof(swap_buffer[0])); i++) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    SDL_QueueAudio(audio_device, samples_to_queue, num_bytes_to_queue);
}

size_t get_frames_remaining() {
    constexpr float buffer_offset_frames = 1.0f;
    uint64_t buffered_byte_count = SDL_GetQueuedAudioSize(audio_device);

    buffered_byte_count = buffered_byte_count * 2 * sample_rate / output_sample_rate / output_channels;

    uint32_t frames_per_vi = (sample_rate / 60);
    if (buffered_byte_count > (buffer_offset_frames * bytes_per_frame * frames_per_vi)) {
        buffered_byte_count -= (buffer_offset_frames * bytes_per_frame * frames_per_vi);
    }
    else {
        buffered_byte_count = 0;
    }
    return static_cast<uint32_t>(buffered_byte_count / bytes_per_frame);
}

void update_audio_converter() {
    if (SDL_BuildAudioCVT(&audio_convert, AUDIO_F32, input_channels, sample_rate, AUDIO_F32, output_channels, output_sample_rate) < 0) {
        printf("Error creating SDL audio converter: %s\n", SDL_GetError());
        throw std::runtime_error("Error creating SDL audio converter");
    }

    discarded_output_frames = duplicated_input_frames * output_sample_rate / sample_rate;
}

void set_frequency(uint32_t freq) {
    sample_rate = freq;
    update_audio_converter();
}

bool reset_audio(uint32_t output_freq) {
    SDL_AudioSpec spec_desired{
        .freq = (int)output_freq,
        .format = AUDIO_F32,
        .channels = (Uint8)output_channels,
        .silence = 0,
        .samples = 0x100,
        .padding = 0,
        .size = 0,
        .callback = nullptr,
        .userdata = nullptr
    };

    audio_device = SDL_OpenAudioDevice(nullptr, false, &spec_desired, nullptr, 0);
    if (audio_device == 0) {
        std::string audio_error = std::string("No audio device could be found. Please make sure an audio device is available.\nError opening audio device: ") + std::string(SDL_GetError());
        recompui::message_box(audio_error.c_str());
        return false;
    }

    SDL_PauseAudioDevice(audio_device, 0);

    output_sample_rate = output_freq;
    update_audio_converter();

    return true;
}

// Graphics tasks go to RT64; only the audio microcode is recompiled (rsp/aspMain.cpp).
extern RspUcodeFunc aspMain;

RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
    case M_AUDTASK:
        return aspMain;

    default:
        fprintf(stderr, "Unknown task: %" PRIu32 "\n", task->t.type);
        return nullptr;
    }
}

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
void slugfest_on_init(uint8_t* rdram, recomp_context* ctx); // src/game/memory_mirror.cpp
gpr get_entrypoint_address();

std::vector<recomp::GameEntry> supported_games = {
    {
        .rom_hash = 0x7DA2C678136D0287ULL,
        .internal_name = "KEN GRIFFEY SLUGFEST",
        .display_name = "Ken Griffey Jr.'s Slugfest",
        .game_id = u8"slugfest.n64.us.1.0",
        .mod_game_id = "slugfest",
        // The game uses a FlashRAM driver (frontend) and also DMAs straight from the cartridge's
        // save address (0x08000000), which the runtime treats as SRAM. Both read the same save
        // buffer, so allow both. Controller Pak saving is emulated separately.
        .save_type = recomp::SaveType::AllowAll,
        .thumbnail_bytes = std::span<const char>(icon_bytes),
        .is_enabled = true,
        .decompression_routine = slugfest::decompress_rom,
        .has_compressed_code = true,
        .entrypoint_address = get_entrypoint_address(),
        .entrypoint = recomp_entrypoint,
        .on_init_callback = slugfest_on_init,
    },
};

std::string get_game_thread_name(const OSThread* t) {
    return "[Game] " + std::to_string(t->id);
}

void enable_texture_pack(recomp::mods::ModContext& context, const recomp::mods::ModHandle& mod) {
    recompui::renderer::enable_texture_pack(context, mod);
}

void disable_texture_pack(recomp::mods::ModContext&, const recomp::mods::ModHandle& mod) {
    recompui::renderer::disable_texture_pack(mod);
}

void reorder_texture_pack(recomp::mods::ModContext&) {
    recompui::renderer::trigger_texture_pack_update();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    recomp::Version project_version{};
    if (!recomp::Version::from_string(version_string, project_version)) {
        ultramodern::error_handling::message_box(("Invalid version string: " + version_string).c_str());
        return EXIT_FAILURE;
    }

    std::srand(std::time(nullptr));

#ifdef _WIN32
    timeBeginPeriod(1);
    SDL_setenv("SDL_AUDIODRIVER", "wasapi", true);
#endif

    NFD_Init();

    recompui::programconfig::set_program_name(slugfest::program_name);
    recompui::programconfig::set_program_id(slugfest::program_id);

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (!reset_audio(48000)) {
        return EXIT_FAILURE;
    }

    std::u8string controller_db_path = (recompui::file::get_program_path() / "recompcontrollerdb.txt").u8string();
    if (SDL_GameControllerAddMappingsFromFile(reinterpret_cast<const char*>(controller_db_path.c_str())) < 0) {
        fprintf(stderr, "Failed to load controller mappings: %s\n", SDL_GetError());
    }

    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");

    recomp::register_config_path(recompui::file::get_app_folder_path());

    for (const auto& game : supported_games) {
        recomp::register_game(game);
    }

    recompui::register_ui_exports();

    slugfest::register_overlays();

    recompinput::players::set_single_player_mode(true);

    slugfest::init_config();

    slugfest::start_memory_watchdog();

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = get_rsp_microcode,
    };

    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = [](uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode) {
            auto presentation_mode = ultramodern::renderer::PresentationMode::PresentEarly;
            return recompui::renderer::create_render_context(rdram, window_handle, presentation_mode, developer_mode);
        },
    };

    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = create_gfx,
        .create_window = create_window,
        .update_gfx = update_gfx,
    };

    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency = set_frequency,
    };

    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = recompinput::poll_inputs,
        .get_input = recompinput::profiles::get_n64_input,
        .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = get_connected_device_info,
    };

    ultramodern::events::callbacks_t thread_callbacks{
        .vi_callback = recompinput::update_rumble,
        .gfx_init_callback = nullptr,
    };

    ultramodern::error_handling::callbacks_t error_handling_callbacks{
        .message_box = recompui::message_box,
    };

    ultramodern::threads::callbacks_t threads_callbacks{
        .get_game_thread_name = get_game_thread_name,
    };

    recomp::mods::ModContentType texture_pack_content_type{
        .content_filename = "rt64.json",
        .allow_runtime_toggle = true,
        .on_enabled = enable_texture_pack,
        .on_disabled = disable_texture_pack,
        .on_reordered = reorder_texture_pack,
    };
    auto texture_pack_content_type_id = recomp::mods::register_mod_content_type(texture_pack_content_type);
    recomp::mods::register_mod_container_type("rtz", std::vector{ texture_pack_content_type_id }, false);

    recomp::start(
        project_version,
        {},
        rsp_callbacks,
        renderer_callbacks,
        audio_callbacks,
        input_callbacks,
        gfx_callbacks,
        thread_callbacks,
        error_handling_callbacks,
        threads_callbacks
    );

    NFD_Quit();

#ifdef _WIN32
    timeEndPeriod(1);
#endif

    return EXIT_SUCCESS;
}
