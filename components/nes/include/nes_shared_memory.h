#ifndef _NES_SHARED_MEMORY_H_
#define _NES_SHARED_MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <event.h>
#include <nes.h>
#include <nesstate.h>
#include <memguard.h>

// Initialize NES shared memory and hardware components
void nes_init_shared_memory(void);

// Free NES shared memory
void nes_free_shared_memory(void);

#ifdef __cplusplus
}
#endif

#endif /* _NES_SHARED_MEMORY_H_ */
