#ifndef PSYZ_ASSERT_H
#define PSYZ_ASSERT_H

#include <assert.h>
#ifndef _MSC_VER
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT(x, ...)
#endif

#endif
