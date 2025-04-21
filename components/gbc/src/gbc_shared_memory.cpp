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

// from lcd.c
extern byte *bgdup; // [256]

void gbc_init_shared_memory(void) {
    // Allocate VRAM in shared memory
    vram_ptr = (uint8_t*)shared_malloc(GBC_VRAM_SIZE);
    if (!vram_ptr) {
        ESP_LOGE(TAG, "Failed to allocate VRAM");
        return;
    }
    memset(vram_ptr, 0, GBC_VRAM_SIZE);

    // Allocate WRAM in shared memory
    wram_ptr = (uint8_t*)shared_malloc(GBC_WRAM_SIZE);
    if (!wram_ptr) {
        ESP_LOGE(TAG, "Failed to allocate WRAM");
        return;
    }
    memset(wram_ptr, 0, GBC_WRAM_SIZE);

    // allocate gbc scan object in shared memory
    scan = (struct gbc_scan*)shared_malloc(sizeof(struct gbc_scan));

    // allocate gbc file buf in shared memory
    gbc_filebuf = (uint8_t*)shared_malloc(4096);

    // allocate CPU structure in shared memory
    cpu = (struct cpu*)shared_malloc(sizeof(struct cpu));

    lcd = (struct lcd*)shared_malloc(sizeof(struct lcd));

    // Allocate audio buffer in shared memory
    audio_ptr = (uint8_t*)shared_malloc(GBC_AUDIO_BUFFER_SIZE);
    if (!audio_ptr) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        return;
    }
    memset(audio_ptr, 0, GBC_AUDIO_BUFFER_SIZE);

    bgdup = (byte*)shared_malloc(256);
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
