#pragma once

#include <stdint.h>
#include <stddef.h>

#include "emuapi.h"

#if defined(__cplusplus)
extern "C" {
#endif
void sms_Init(void);
void sms_DeInit(void);
void sms_Step(void);
void sms_Start(uint8_t *romdata, size_t rom_data_size);
void gg_Start(uint8_t *romdata, size_t rom_data_size);
void sms_Input(int click);
#if defined(__cplusplus)
}
#endif
