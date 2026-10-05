/*
 * p8_lua.h
 *
 *  Created on: Dec 13, 2023
 *      Author: bbaker
 */

#ifndef P8_LUA_H
#define P8_LUA_H

#include <stdint.h>
#include <stdbool.h>

int lua_load_api();
int lua_shutdown_api();
void lua_print_error();
int lua_init_script(const char *file_name, const char *script);
int lua_call_function(const char *name, int ret);
int lua_update();
int lua_draw();
int lua_init();
bool lua_has_main_loop_callbacks();
void lua_get_error(const char **err_type, char *err, int err_size, const char **filename, int *lineno);

extern char m_str_buffer[256];

#endif
