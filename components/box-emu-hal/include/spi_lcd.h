// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <stdint.h>
#include <stddef.h>

//*****************************************************************************
//
// Make sure all of the definitions in this header have a C binding.
//
//*****************************************************************************

#ifdef __cplusplus
extern "C"
{
#endif

#define NUM_ROWS_IN_FRAME_BUFFER 50

uint16_t make_color(uint8_t r, uint8_t g, uint8_t b);
void set_pixel(const uint16_t x, const uint16_t y, const uint16_t color);
uint16_t* get_vram0();
uint16_t* get_vram1();
uint8_t* get_frame_buffer0();
uint8_t* get_frame_buffer1();
void delay_us(size_t num_us);
uint16_t reorder_color(uint16_t color);
void lcd_set_polling_transmit();
void lcd_set_queued_transmit();
void lcd_set_drawing_frame(const uint16_t xs, const uint16_t ys, const uint16_t width, const uint16_t height);
void lcd_continue_writing(const uint8_t *buffer, size_t buffer_length);
void lcd_write_frame(const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height, const uint8_t *data);
void lcd_init();

#ifdef __cplusplus
}
#endif

