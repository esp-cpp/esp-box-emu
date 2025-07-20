#include "pico8.hpp"
#include "logger.hpp"

// Include femto8 headers (would be in femto8 submodule)
extern "C" {
// These would be the actual femto8 API functions
// #include "femto8/pico8.h"
// #include "femto8/cart.h"
// #include "femto8/video.h"
// #include "femto8/audio.h"
}

static espp::Logger logger({.tag = "pico8", .level = espp::Logger::Verbosity::INFO});

// Static variables for emulator state
static bool pico8_initialized = false;
static uint8_t* video_buffer = nullptr;
static uint8_t current_buttons = 0;
static bool audio_enabled = true;

// PICO-8 16-color palette (RGB565 format for ESP32 display)
static const uint16_t pico8_palette[16] = {
    0x0000, // 0: Black
    0x1947, // 1: Dark Blue
    0x7827, // 2: Dark Purple
    0x0478, // 3: Dark Green
    0xA800, // 4: Brown
    0x5AA9, // 5: Dark Grey
    0xC618, // 6: Light Grey
    0xFFFF, // 7: White
    0xF800, // 8: Red
    0xFD00, // 9: Orange
    0xFFE0, // 10: Yellow
    0x0F80, // 11: Green
    0x2D7F, // 12: Blue
    0x83B3, // 13: Indigo
    0xFBB5, // 14: Pink
    0xFED7  // 15: Peach
};

void init_pico8(const char* rom_filename, const uint8_t* rom_data, size_t rom_size) {
    logger.info("Initializing PICO-8 emulator");
    
    if (pico8_initialized) {
        logger.warn("PICO-8 already initialized, deinitializing first");
        deinit_pico8();
    }
    
    // Allocate video buffer (128x128 pixels, 1 byte per pixel for palette index)
    video_buffer = (uint8_t*)malloc(128 * 128);
    if (!video_buffer) {
        logger.error("Failed to allocate video buffer");
        return;
    }
    
    // Clear video buffer
    memset(video_buffer, 0, 128 * 128);
    
    // Initialize femto8 core
    // femto8_init();
    
    // Load cartridge
    if (rom_data && rom_size > 0) {
        logger.info("Loading PICO-8 cartridge: {} ({} bytes)", rom_filename, rom_size);
        // femto8_load_cart(rom_data, rom_size);
    }
    
    pico8_initialized = true;
    logger.info("PICO-8 emulator initialized successfully");
}

void deinit_pico8() {
    if (!pico8_initialized) {
        return;
    }
    
    logger.info("Deinitializing PICO-8 emulator");
    
    // Cleanup femto8 core
    // femto8_deinit();
    
    // Free video buffer
    if (video_buffer) {
        free(video_buffer);
        video_buffer = nullptr;
    }
    
    pico8_initialized = false;
    logger.info("PICO-8 emulator deinitialized");
}

void reset_pico8() {
    if (!pico8_initialized) {
        logger.warn("PICO-8 not initialized");
        return;
    }
    
    logger.info("Resetting PICO-8 emulator");
    // femto8_reset();
}

void run_pico8_frame() {
    if (!pico8_initialized) {
        return;
    }
    
    // Update input state in femto8
    // femto8_set_buttons(current_buttons);
    
    // Run one frame of emulation
    // femto8_run_frame();
    
    // Get updated video buffer from femto8
    // const uint8_t* femto8_framebuffer = femto8_get_framebuffer();
    // if (femto8_framebuffer && video_buffer) {
    //     memcpy(video_buffer, femto8_framebuffer, 128 * 128);
    // }
}

std::span<uint8_t> get_pico8_video_buffer() {
    if (!video_buffer) {
        return std::span<uint8_t>();
    }
    return std::span<uint8_t>(video_buffer, 128 * 128);
}

int get_pico8_screen_width() {
    return 128;
}

int get_pico8_screen_height() {
    return 128;
}

void set_pico8_input(uint8_t buttons) {
    current_buttons = buttons;
}

void pico8_key_down(int key) {
    // Handle individual key presses
    // This would map to PICO-8's keyboard input system
    // femto8_key_down(key);
}

void pico8_key_up(int key) {
    // Handle individual key releases
    // femto8_key_up(key);
}

void get_pico8_audio_buffer(int16_t* buffer, size_t frames) {
    if (!pico8_initialized || !audio_enabled || !buffer) {
        // Fill with silence if not initialized or disabled
        memset(buffer, 0, frames * 2 * sizeof(int16_t)); // Stereo
        return;
    }
    
    // Get audio samples from femto8
    // femto8_get_audio_samples(buffer, frames);
    
    // For now, just fill with silence
    memset(buffer, 0, frames * 2 * sizeof(int16_t));
}

void set_pico8_audio_enabled(bool enabled) {
    audio_enabled = enabled;
    logger.info("PICO-8 audio {}", enabled ? "enabled" : "disabled");
}

bool load_pico8_state(const char* path) {
    if (!pico8_initialized || !path) {
        return false;
    }
    
    logger.info("Loading PICO-8 save state: {}", path);
    // return femto8_load_state(path);
    return true; // Placeholder
}

bool save_pico8_state(const char* path) {
    if (!pico8_initialized || !path) {
        return false;
    }
    
    logger.info("Saving PICO-8 save state: {}", path);
    // return femto8_save_state(path);
    return true; // Placeholder
}

bool load_pico8_cartridge(const uint8_t* data, size_t size) {
    if (!pico8_initialized || !data || size == 0) {
        return false;
    }
    
    logger.info("Loading PICO-8 cartridge ({} bytes)", size);
    // return femto8_load_cart(data, size);
    return true; // Placeholder
}

const char* get_pico8_cartridge_title() {
    if (!pico8_initialized) {
        return "Unknown";
    }
    
    // Extract title from loaded cartridge
    // return femto8_get_cart_title();
    return "PICO-8 Game"; // Placeholder
}