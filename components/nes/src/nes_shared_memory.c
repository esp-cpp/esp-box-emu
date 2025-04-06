#include "nes_shared_memory.h"
#include "nes_external.h"
#include "shared_memory.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "nes_shared_memory";

// Define the external nes variable in shared memory
nes_t *nes_context;

/* allocates memory and clears it */
void *_my_malloc(int size)
{
    // get a pointer to the memory pool
    shared_mem_request_t request = {
        .size = size,
        .region = SHARED_MEM_DEFAULT
    };
    uint8_t *ptr = shared_mem_allocate(&request);
    return ptr;
}

/* free a pointer allocated with my_malloc */
void _my_free(void **data)
{
}

nes_t* nes_init_shared_memory(void) {
    nes_cpu = (nes6502_context *)_my_malloc(sizeof(nes6502_context));

    nes_context = nes_create();

    if (!nes_context) {
        ESP_LOGE("nes_init_shared_memory", "Failed to allocate NES state");
        return NULL;
    }

    return nes_context;
}

void nes_free_shared_memory(void) {
    shared_mem_clear();
}
