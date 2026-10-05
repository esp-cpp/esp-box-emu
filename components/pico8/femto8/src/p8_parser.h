/*
 * p8_parser.h
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#ifndef P8_PARSER_H
#define P8_PARSER_H

#include <stdint.h>

int parse_cart_ram(uint8_t *buffer, int size, uint8_t *memory, uint8_t *decompression_buffer, char *lua_script, uint8_t *label_image);
int parse_cart_file(const char *file_name, uint8_t *memory, uint8_t *file_buffer, uint8_t *decompression_buffer, char *lua_script, uint8_t *label_image);
int parse_png_ram(const char *file_name, uint8_t *buffer, int file_size, uint8_t *memory, uint8_t *decompression_buffer, const char **lua_script, uint8_t *label_image);

#endif
