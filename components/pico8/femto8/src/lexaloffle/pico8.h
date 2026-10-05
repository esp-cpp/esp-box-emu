#ifndef PICO8_H
#define PICO8_H

#include <stdint.h>

int decompress_mini(uint8_t *in_p, uint8_t *out_p, int max_len);
int is_compressed_format_header(uint8_t *dat);
// max_len should be 0x10000 (64k max code size)
// out_p should allocate 0x10001 (includes null terminator)
int pico8_code_section_decompress(uint8_t *in_p, uint8_t *out_p, int max_len);
int pxa_decompress(uint8_t *in_p, uint8_t *out_p, int max_len);

#endif
