#pragma once

#if CONFIG_HARDWARE_BOX
#include "box.hpp"
#elif CONFIG_HARDWARE_BOX_3
#include "box_3.hpp"
#else
#error "Invalid module selection"
#endif

#if CONFIG_HARDWARE_V0
#include "emu_v0.hpp"
#elif CONFIG_HARDWARE_V1
#include "emu_v1.hpp"
#else
#error "Invalid hardware version"
#endif
