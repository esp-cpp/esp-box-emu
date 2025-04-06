#include "gbc_shared_memory.hpp"
#include "shared_memory.h"
#include "esp_log.h"

#include <cstring>

extern "C" {
#include <gnuboy/loader.h>
#include <gnuboy/hw.h>
#include <gnuboy/lcd.h>
#include <gnuboy/fb.h>
#include <gnuboy/cpu.h>
#include <gnuboy/mem.h>
#include <gnuboy/sound.h>
#include <gnuboy/pcm.h>
#include <gnuboy/regs.h>
#include <gnuboy/rtc.h>
#include <gnuboy/gnuboy.h>
}

static const char *TAG = "gbc_shared_memory";

// GBC memory sizes
#define GBC_VRAM_SIZE (16 * 1024)  // 16KB VRAM
#define GBC_WRAM_SIZE (32 * 1024)  // 32KB WRAM
#define GBC_AUDIO_BUFFER_SIZE (2048 * sizeof(int16_t))  // 2048 samples at 16-bit per sample

// Static pointers to shared memory regions
static uint8_t* vram_ptr = nullptr;
static uint8_t* wram_ptr = nullptr;
static uint8_t* audio_ptr = nullptr;
struct cpu *cpu = nullptr;

void gbc_init_shared_memory(void) {
    // Allocate VRAM in shared memory
    shared_mem_request_t vram_request = {
        .size = GBC_VRAM_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    vram_ptr = (uint8_t*)shared_mem_allocate(&vram_request);
    if (!vram_ptr) {
        ESP_LOGE(TAG, "Failed to allocate VRAM");
        return;
    }
    memset(vram_ptr, 0, GBC_VRAM_SIZE);

    // Allocate WRAM in shared memory
    shared_mem_request_t wram_request = {
        .size = GBC_WRAM_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    wram_ptr = (uint8_t*)shared_mem_allocate(&wram_request);
    if (!wram_ptr) {
        ESP_LOGE(TAG, "Failed to allocate WRAM");
        return;
    }
    memset(wram_ptr, 0, GBC_WRAM_SIZE);

    // allocate gbc scan object in shared memory
    shared_mem_request_t gbc_scan_request = {
        .size = sizeof(struct gbc_scan),
        .region = SHARED_MEM_DEFAULT
    };
    scan = (struct gbc_scan*)shared_mem_allocate(&gbc_scan_request);

    // allocate gbc file buf in shared memory
    shared_mem_request_t gbc_file_request = {
        .size = 4096,
        .region = SHARED_MEM_DEFAULT
    };
    gbc_filebuf = (uint8_t*)shared_mem_allocate(&gbc_file_request);

    // allocate CPU structure in shared memory
    shared_mem_request_t cpu_request = {
        .size = sizeof(struct cpu),
        .region = SHARED_MEM_DEFAULT
    };
    cpu = (struct cpu*)shared_mem_allocate(&cpu_request);

    // Allocate audio buffer in shared memory
    shared_mem_request_t audio_request = {
        .size = GBC_AUDIO_BUFFER_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    audio_ptr = (uint8_t*)shared_mem_allocate(&audio_request);
    if (!audio_ptr) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return;
    }
    memset(audio_ptr, 0, GBC_AUDIO_BUFFER_SIZE);
}

void gbc_free_shared_memory(void) {
    // Clear all shared memory
    shared_mem_clear();
    
    // Reset pointers
    vram_ptr = nullptr;
    wram_ptr = nullptr;
    audio_ptr = nullptr;
}

void gbc_get_memory_regions(uint8_t** vram, uint8_t** wram, uint8_t** audio) {
    if (vram) *vram = vram_ptr;
    if (wram) *wram = wram_ptr;
    if (audio) *audio = audio_ptr;
}
