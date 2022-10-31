#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_init();
void audio_play_frame(uint8_t *data, uint32_t num_bytes);

#ifdef __cplusplus
}
#endif
