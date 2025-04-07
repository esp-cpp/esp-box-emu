#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

void z80_init_shared_memory(void);
void z80_free_shared_memory(void);
void z80_get_memory_regions(uint8_t** cycles_table, uint8_t** cycles_cb_table, uint8_t** cycles_ed_table, 
                           uint8_t** cycles_xx_table, uint8_t** cycles_xxcb_table, uint8_t** zs_table,
                           uint8_t** pzs_table, uint16_t** daa_table);

#ifdef __cplusplus
}
#endif
