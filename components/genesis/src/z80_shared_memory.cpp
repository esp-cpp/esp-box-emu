#include "z80_shared_memory.hpp"
#include "shared_memory.h"
#include "esp_log.h"

static const char* TAG = "z80_shared_memory";

// Memory sizes
static constexpr size_t Z80_CYCLES_SIZE = 256;
static constexpr size_t Z80_CYCLES_CB_SIZE = 256;
static constexpr size_t Z80_CYCLES_ED_SIZE = 256;
static constexpr size_t Z80_CYCLES_XX_SIZE = 256;
static constexpr size_t Z80_CYCLES_XXCB_SIZE = 256;
static constexpr size_t Z80_ZS_TABLE_SIZE = 256;
static constexpr size_t Z80_PZS_TABLE_SIZE = 256;
static constexpr size_t Z80_DAA_TABLE_SIZE = 2048;

// Static pointers to shared memory regions
static uint8_t* cycles = nullptr;
static uint8_t* cycles_cb = nullptr;
static uint8_t* cycles_ed = nullptr;
static uint8_t* cycles_xx = nullptr;
static uint8_t* cycles_xxcb = nullptr;
static uint8_t* zs_table = nullptr;
static uint8_t* pzs_table = nullptr;
static uint16_t* daa_table = nullptr;

void z80_init_shared_memory() {
    // Allocate memory for each table
    shared_mem_request_t cycles_request = {
        .size = Z80_CYCLES_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    cycles = static_cast<uint8_t*>(shared_mem_allocate(&cycles_request));
    if (!cycles) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 cycles table");
        return;
    }

    shared_mem_request_t cycles_cb_request = {
        .size = Z80_CYCLES_CB_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    cycles_cb = static_cast<uint8_t*>(shared_mem_allocate(&cycles_cb_request));
    if (!cycles_cb) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 cycles CB table");
        return;
    }

    shared_mem_request_t cycles_ed_request = {
        .size = Z80_CYCLES_ED_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    cycles_ed = static_cast<uint8_t*>(shared_mem_allocate(&cycles_ed_request));
    if (!cycles_ed) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 cycles ED table");
        return;
    }

    shared_mem_request_t cycles_xx_request = {
        .size = Z80_CYCLES_XX_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    cycles_xx = static_cast<uint8_t*>(shared_mem_allocate(&cycles_xx_request));
    if (!cycles_xx) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 cycles XX table");
        return;
    }

    shared_mem_request_t cycles_xxcb_request = {
        .size = Z80_CYCLES_XXCB_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    cycles_xxcb = static_cast<uint8_t*>(shared_mem_allocate(&cycles_xxcb_request));
    if (!cycles_xxcb) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 cycles XXCB table");
        return;
    }

    shared_mem_request_t zs_table_request = {
        .size = Z80_ZS_TABLE_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    zs_table = static_cast<uint8_t*>(shared_mem_allocate(&zs_table_request));
    if (!zs_table) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 ZS table");
        return;
    }

    shared_mem_request_t pzs_table_request = {
        .size = Z80_PZS_TABLE_SIZE,
        .region = SHARED_MEM_DEFAULT
    };
    pzs_table = static_cast<uint8_t*>(shared_mem_allocate(&pzs_table_request));
    if (!pzs_table) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 PZS table");
        return;
    }

    shared_mem_request_t daa_table_request = {
        .size = Z80_DAA_TABLE_SIZE * sizeof(uint16_t),
        .region = SHARED_MEM_DEFAULT
    };
    daa_table = static_cast<uint16_t*>(shared_mem_allocate(&daa_table_request));
    if (!daa_table) {
        ESP_LOGE(TAG, "Failed to allocate memory for Z80 DAA table");
        return;
    }
}

void z80_free_shared_memory() {
    // Clear all shared memory
    shared_mem_clear();
    
    // Reset pointers
    cycles = nullptr;
    cycles_cb = nullptr;
    cycles_ed = nullptr;
    cycles_xx = nullptr;
    cycles_xxcb = nullptr;
    zs_table = nullptr;
    pzs_table = nullptr;
    daa_table = nullptr;
}

void z80_get_memory_regions(uint8_t** cycles_out, uint8_t** cycles_cb_out, uint8_t** cycles_ed_out,
                           uint8_t** cycles_xx_out, uint8_t** cycles_xxcb_out, uint8_t** zs_table_out,
                           uint8_t** pzs_table_out, uint16_t** daa_table_out) {
    // Output pointers to memory regions
    *cycles_out = cycles;
    *cycles_cb_out = cycles_cb;
    *cycles_ed_out = cycles_ed;
    *cycles_xx_out = cycles_xx;
    *cycles_xxcb_out = cycles_xxcb;
    *zs_table_out = zs_table;
    *pzs_table_out = pzs_table;
    *daa_table_out = daa_table;
} 