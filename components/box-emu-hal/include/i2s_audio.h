#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_SAMPLE_RATE (32000)
#define AUDIO_BUFFER_SIZE (AUDIO_SAMPLE_RATE / 5)
#define AUDIO_SAMPLE_COUNT (AUDIO_SAMPLE_RATE / 60)

void audio_init();
int16_t* get_audio_buffer0();
int16_t* get_audio_buffer1();
void audio_play_frame(const uint8_t *data, uint32_t num_bytes);

bool is_muted();
void set_muted(bool mute);

int get_audio_volume();
void set_audio_volume(int percent);

#ifdef __cplusplus
}
#endif
