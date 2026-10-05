/*
 * p8_audio.h
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#include <stdint.h>

#ifndef P8_AUDIO_H
#define P8_AUDIO_H

#define SAMPLE_RATE 44100
#define MAX_VOLUME 4096
#define CHANNEL_COUNT 4
#define SOUND_BUFFER_SIZE 2048
#define SOUND_COUNT 64
#define MUSIC_COUNT 64
#define SOUND_QUEUE_SIZE 8

void audio_init();
void audio_resume();
void audio_pause();
void audio_close();
void audio_sound(int32_t index, int32_t channel, uint32_t start, uint32_t end);
void audio_music(int32_t index, int32_t fade_ms, int32_t mask);
int32_t audio_stat(int32_t index);
void audio_pcm_write(unsigned address, unsigned length);
int16_t audio_pcm_buffered();
int16_t audio_pcm_app_buffer();

#endif