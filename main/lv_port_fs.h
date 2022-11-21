/**
 * @file lv_port_fs_templ.h
 *
 */

#pragma once

#include "fs_init.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "sdkconfig.h"
#include <stdio.h>
#include "esp_err.h"
#include "lvgl/lvgl.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**
 * @brief Init LVGL file system.
 * 
 * @return esp_err_t Result of state.
 */
esp_err_t lv_port_fs_init(void);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif
