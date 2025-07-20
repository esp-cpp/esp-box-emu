#include "pico8.hpp"
#include "logger.hpp"
#include "shared_memory.hpp"

// Include femto8 headers (direct source integration)
extern "C" {
#include "femto8/pico8.h"
#include "femto8/pico8_types.h"
#include "femto8/pico8_config.h"
}

static espp::Logger logger({.tag = "pico8", .level = espp::Logger::Verbosity::INFO});

// Static variables for emulator state
static bool pico8_initialized = false;
static uint8_t* video_buffer = nullptr;
static uint8_t current_buttons = 0;
static bool audio_enabled = true;

// Memory management using shared_memory component
static uint8_t* pico8_ram = nullptr;
static uint8_t* pico8_vram = nullptr;
static uint8_t* lua_heap = nullptr;
static uint8_t* audio_buffers = nullptr;

// Memory sizes
constexpr size_t PICO8_RAM_SIZE = 32768;      // 32KB - PICO-8 RAM
constexpr size_t PICO8_VRAM_SIZE = 16384;     // 16KB - Video RAM  
constexpr size_t LUA_HEAP_SIZE = 65536;       // 64KB - Lua heap
constexpr size_t AUDIO_BUFFER_SIZE = 8192;    // 8KB - Audio buffers

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

// Initialize memory using shared_memory component
static bool init_pico8_memory() {
    logger.info("Initializing PICO-8 memory using shared_memory component");
    
    // Allocate memory blocks using shared_memory
    pico8_ram = SharedMemory::get().allocate(PICO8_RAM_SIZE, "pico8_ram");
    if (!pico8_ram) {
        logger.error("Failed to allocate PICO-8 RAM");
        return false;
    }
    
    pico8_vram = SharedMemory::get().allocate(PICO8_VRAM_SIZE, "pico8_vram");
    if (!pico8_vram) {
        logger.error("Failed to allocate PICO-8 VRAM");
        return false;
    }
    
    lua_heap = SharedMemory::get().allocate(LUA_HEAP_SIZE, "pico8_lua");
    if (!lua_heap) {
        logger.error("Failed to allocate Lua heap");
        return false;
    }
    
    audio_buffers = SharedMemory::get().allocate(AUDIO_BUFFER_SIZE, "pico8_audio");
    if (!audio_buffers) {
        logger.error("Failed to allocate audio buffers");
        return false;
    }
    
    // Clear all allocated memory
    memset(pico8_ram, 0, PICO8_RAM_SIZE);
    memset(pico8_vram, 0, PICO8_VRAM_SIZE);
    memset(lua_heap, 0, LUA_HEAP_SIZE);
    memset(audio_buffers, 0, AUDIO_BUFFER_SIZE);
    
    logger.info("PICO-8 memory allocation successful:");
    logger.info("  RAM: {} bytes at {:p}", PICO8_RAM_SIZE, (void*)pico8_ram);
    logger.info("  VRAM: {} bytes at {:p}", PICO8_VRAM_SIZE, (void*)pico8_vram);
    logger.info("  Lua Heap: {} bytes at {:p}", LUA_HEAP_SIZE, (void*)lua_heap);
    logger.info("  Audio: {} bytes at {:p}", AUDIO_BUFFER_SIZE, (void*)audio_buffers);
    
    return true;
}

static void deinit_pico8_memory() {
    logger.info("Deallocating PICO-8 memory");
    
    if (pico8_ram) {
        SharedMemory::get().deallocate(pico8_ram, "pico8_ram");
        pico8_ram = nullptr;
    }
    
    if (pico8_vram) {
        SharedMemory::get().deallocate(pico8_vram, "pico8_vram");
        pico8_vram = nullptr;
    }
    
    if (lua_heap) {
        SharedMemory::get().deallocate(lua_heap, "pico8_lua");
        lua_heap = nullptr;
    }
    
    if (audio_buffers) {
        SharedMemory::get().deallocate(audio_buffers, "pico8_audio");
        audio_buffers = nullptr;
    }
}

void init_pico8(const char* rom_filename, const uint8_t* rom_data, size_t rom_size) {
    logger.info("Initializing PICO-8 emulator");
    
    if (pico8_initialized) {
        logger.warn("PICO-8 already initialized, deinitializing first");
        deinit_pico8();
    }
    
    // Initialize memory management
    if (!init_pico8_memory()) {
        logger.error("Failed to initialize PICO-8 memory");
        return;
    }
    
    // Set up video buffer (using VRAM allocation)
    video_buffer = pico8_vram;
    
    // Initialize femto8 core with our memory allocations
    femto8_config_t config = {
        .ram = pico8_ram,
        .ram_size = PICO8_RAM_SIZE,
        .vram = pico8_vram,
        .vram_size = PICO8_VRAM_SIZE,
        .lua_heap = lua_heap,
        .lua_heap_size = LUA_HEAP_SIZE,
        .audio_buffer = audio_buffers,
        .audio_buffer_size = AUDIO_BUFFER_SIZE
    };
    
    if (femto8_init(&config) != 0) {
        logger.error("Failed to initialize femto8 core");
        deinit_pico8_memory();
        return;
    }
    
    // Load cartridge
    if (rom_data && rom_size > 0) {
        logger.info("Loading PICO-8 cartridge: {} ({} bytes)", rom_filename, rom_size);
        if (femto8_load_cart(rom_data, rom_size) != 0) {
            logger.error("Failed to load PICO-8 cartridge");
            deinit_pico8();
            return;
        }
    }
    
    pico8_initialized = true;
    logger.info("PICO-8 emulator initialized successfully");
    logger.info("Total memory usage: {} KB", 
                (PICO8_RAM_SIZE + PICO8_VRAM_SIZE + LUA_HEAP_SIZE + AUDIO_BUFFER_SIZE) / 1024);
}

void deinit_pico8() {
    if (!pico8_initialized) {
        return;
    }
    
    logger.info("Deinitializing PICO-8 emulator");
    
    // Cleanup femto8 core
    femto8_deinit();
    
    // Free memory allocations
    deinit_pico8_memory();
    
    video_buffer = nullptr;
    pico8_initialized = false;
    logger.info("PICO-8 emulator deinitialized");
}

void reset_pico8() {
    if (!pico8_initialized) {
        logger.warn("PICO-8 not initialized");
        return;
    }
    
    logger.info("Resetting PICO-8 emulator");
    femto8_reset();
}

void run_pico8_frame() {
    if (!pico8_initialized) {
        return;
    }
    
    // Update input state in femto8
    femto8_set_buttons(current_buttons);
    
    // Run one frame of emulation
    femto8_run_frame();
    
    // Video buffer is automatically updated by femto8 into our VRAM
    // No need to copy since we're using shared memory
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
    if (!pico8_initialized) return;
    femto8_key_down(key);
}

void pico8_key_up(int key) {
    if (!pico8_initialized) return;
    femto8_key_up(key);
}

void get_pico8_audio_buffer(int16_t* buffer, size_t frames) {
    if (!pico8_initialized || !audio_enabled || !buffer) {
        // Fill with silence if not initialized or disabled
        memset(buffer, 0, frames * 2 * sizeof(int16_t)); // Stereo
        return;
    }
    
    // Get audio samples from femto8
    femto8_get_audio_samples(buffer, frames);
}

void set_pico8_audio_enabled(bool enabled) {
    audio_enabled = enabled;
    logger.info("PICO-8 audio {}", enabled ? "enabled" : "disabled");
    
    if (pico8_initialized) {
        femto8_set_audio_enabled(enabled);
    }
}

bool load_pico8_state(const char* path) {
    if (!pico8_initialized || !path) {
        return false;
    }
    
    logger.info("Loading PICO-8 save state: {}", path);
    return femto8_load_state(path) == 0;
}

bool save_pico8_state(const char* path) {
    if (!pico8_initialized || !path) {
        return false;
    }
    
    logger.info("Saving PICO-8 save state: {}", path);
    return femto8_save_state(path) == 0;
}

bool load_pico8_cartridge(const uint8_t* data, size_t size) {
    if (!pico8_initialized || !data || size == 0) {
        return false;
    }
    
    logger.info("Loading PICO-8 cartridge ({} bytes)", size);
    return femto8_load_cart(data, size) == 0;
}

const char* get_pico8_cartridge_title() {
    if (!pico8_initialized) {
        return "Unknown";
    }
    
    // Extract title from loaded cartridge
    const char* title = femto8_get_cart_title();
    return title ? title : "PICO-8 Game";
}

// ESP32-specific optimizations
#ifdef FEMTO8_ESP32
// Memory statistics for debugging
void get_pico8_memory_stats() {
    if (!pico8_initialized) {
        logger.warn("PICO-8 not initialized");
        return;
    }
    
    logger.info("PICO-8 Memory Statistics:");
    logger.info("  RAM usage: {}/{} bytes ({:.1f}%)", 
                femto8_get_ram_usage(), PICO8_RAM_SIZE, 
                (float)femto8_get_ram_usage() / PICO8_RAM_SIZE * 100.0f);
    logger.info("  Lua heap usage: {}/{} bytes ({:.1f}%)", 
                femto8_get_lua_heap_usage(), LUA_HEAP_SIZE,
                (float)femto8_get_lua_heap_usage() / LUA_HEAP_SIZE * 100.0f);
}

// Performance monitoring
void get_pico8_performance_stats() {
    if (!pico8_initialized) {
        return;
    }
    
    uint32_t frame_time = femto8_get_frame_time_us();
    uint32_t cpu_usage = femto8_get_cpu_usage_percent();
    
    logger.info("PICO-8 Performance: {}μs/frame, {}% CPU", frame_time, cpu_usage);
}
#endif