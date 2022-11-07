#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_init();
int16_t* get_audio_buffer();
void audio_play_frame(uint8_t *data, uint32_t num_bytes);
void set_audio_volume(int percent);

#ifdef __cplusplus
}
#endif
