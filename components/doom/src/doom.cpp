#include "doom.hpp"
#include "box-emu.hpp"
#include "statistics.hpp"
#include "esp_log.h"

// prboom includes
extern "C" {
#include "prboom/d_main.h"
#include "prboom/i_system.h"
#include "prboom/i_video.h"
#include "prboom/i_sound.h"
#include "prboom/doomdef.h"
#include "prboom/doomtype.h"
#include "prboom/v_video.h"
#include "prboom/w_wad.h"
#include "prboom/g_game.h"
}

static const char* TAG = "DOOM";

static bool initialized = false;
static bool unlock = false;
static int frame_counter = 0;

// Video mode settings
static std::atomic<bool> scaled = false;
static std::atomic<bool> filled = true;

void set_doom_video_original() {
    scaled = false;
    filled = false;
    BoxEmu::get().native_size(320, 200);
}

void set_doom_video_fit() {
    scaled = true;
    filled = false;
    BoxEmu::get().native_size(320, 200);
}

void set_doom_video_fill() {
    scaled = false;
    filled = true;
    BoxEmu::get().native_size(320, 200);
}

void init_doom(const std::string& wad_filename, uint8_t *wad_data, size_t wad_data_size) {
    if (!initialized) {
        // Initialize system interface
        if (!I_StartDisplay()) {
            ESP_LOGE(TAG, "Failed to initialize display");
            return;
        }

        // Initialize graphics
        I_InitGraphics();

        // Set native size and palette
        BoxEmu::get().native_size(320, 200);
        BoxEmu::get().palette(nullptr);

        // Set audio sample rate
        BoxEmu::get().audio_sample_rate(44100);

        // Initialize sound
        I_InitSound();

        initialized = true;
    }

    // Load WAD file
    if (!D_AddFile(wad_filename.c_str())) {
        ESP_LOGE(TAG, "Failed to load WAD file");
        return;
    }

    // Initialize game
    D_DoomMainSetup();
    D_DoomMain();

    frame_counter = 0;
    reset_frame_time();
}

void run_doom_rom() {
    auto start = esp_timer_get_time();

    // Handle input
    auto state = BoxEmu::get().gamepad_state();
    
    // Map gamepad to Doom controls
    if (state.up) I_HandleKey(KEY_UPARROW);
    if (state.down) I_HandleKey(KEY_DOWNARROW);
    if (state.left) I_HandleKey(KEY_LEFTARROW);
    if (state.right) I_HandleKey(KEY_RIGHTARROW);
    if (state.a) I_HandleKey(KEY_FIRE);
    if (state.b) I_HandleKey(KEY_USE);

    // Run one frame
    if ((frame_counter % 2) == 0) {
        // Start frame
        I_StartFrame();
        
        // Update game state
        D_DoomLoop();
        
        // Finish frame
        I_FinishUpdate();
        
        // Push frame to display
        BoxEmu::get().push_frame((uint8_t*)I_VideoBuffer);
    }

    // Update frame counter
    frame_counter++;

    // Handle timing
    auto end = esp_timer_get_time();
    uint64_t elapsed = end - start;
    update_frame_time(elapsed);
    static constexpr uint64_t max_frame_time = 1000000 / 60;
    if (!unlock && elapsed < max_frame_time) {
        auto sleep_time = (max_frame_time - elapsed) / 1e3;
        std::this_thread::sleep_for(sleep_time * std::chrono::milliseconds(1));
    }
}

void load_doom(std::string_view save_path) {
    if (save_path.size()) {
        G_LoadGame(save_path.data());
    }
}

void save_doom(std::string_view save_path) {
    if (save_path.size()) {
        G_SaveGame(save_path.data());
    }
}

std::vector<uint8_t> get_doom_video_buffer() {
    std::vector<uint8_t> frame(320 * 200 * 2);
    memcpy(frame.data(), I_VideoBuffer, frame.size());
    return frame;
}

void start_doom_tasks() {
    doom_resume_video_task();
    doom_resume_audio_task();
}

void stop_doom_tasks() {
    doom_pause_video_task();
    doom_pause_audio_task();
}

void deinit_doom() {
    if (initialized) {
        // Shutdown graphics
        I_ShutdownGraphics();
        
        // End display
        I_EndDisplay();
        
        initialized = false;
    }
    BoxEmu::get().audio_sample_rate(48000);
}
