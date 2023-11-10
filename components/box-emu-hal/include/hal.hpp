#pragma once

#include <sdkconfig.h>

#if defined(CONFIG_HARDWARE_BOX)
#include "box.hpp"
#elif defined(CONFIG_HARDWARE_BOX_3)
#include "box_3.hpp"
#else
#error "Unsupported hardware configuration specified"
#endif
