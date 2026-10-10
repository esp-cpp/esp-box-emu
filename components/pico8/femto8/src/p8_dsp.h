/*
 * p8_dsp.h
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#ifndef P8_DSP_H
#define P8_DSP_H

#include <stdint.h>

void dsp_square_wave(uint32_t frequency, int16_t amplitude, int16_t offset, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_pulse_wave(uint32_t frequency, int16_t amplitude, int16_t offset, float duty_cycle, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_triangle_wave(uint32_t frequency, int16_t amplitude, int16_t offset, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_sawtooth_wave(uint32_t frequency, int16_t amplitude, int16_t offset, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_tilted_sawtooth_wave(uint32_t frequency, int16_t amplitude, int16_t offset, float duty_cycle, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_organ_wave(uint32_t frequency, int16_t amplitude, int16_t offset, float coefficient, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_noise(uint32_t frequency, int16_t amplitude, int position, int dest_offset, int dest_length, int16_t *dest);
void dsp_fade_in(int16_t amplitude, int dest_offset, int dest_length, int16_t *dest);
void dsp_fade_out(int16_t amplitude, int dest_offset, int dest_length, int16_t *dest);

#endif