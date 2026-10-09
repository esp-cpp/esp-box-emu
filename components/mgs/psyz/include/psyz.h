#ifndef PSYZ_H
#define PSYZ_H

/**
 * @file psyz.h
 * @brief Main header to include when re-compiling a PS1 application targeting
 *        foreign platforms. It contains all publicly exposed APIs from PsyZ.
 *
 * All public API of PsyZ use the prefix `Psyz_`, while type use prefix `Psyz`.
 *
 * Functions are guaranteed to be platform-agnostic and exist for the sole
 * purpose of emulating certain PlayStation hardware characteristics. Typically
 * this is a lightweight layer that decodes hardware registers, manages state,
 * and ingests commands. Events are then dispatched to the relevant subsystem
 * backend. Examples: `Psyz_Cd`, `Psyz_Spu`, etc.
 *
 * APIs that implement the backend of a specific subsystem for the target
 * platform use the prefix `Psyz_{subsystem}`. These are the lowest layer of
 * abstraction of PsyZ, and they generally communicate with the platform
 * library or the hardware directly. When targeting a new platform, all the
 * subsystem endpoints are required to be implemented to guarantee full
 * functionality. All micro-optimizations and the use of hardware-specific
 * quirks live here. Examples: `Psyz_Audio`, `Psyz_Video`, etc.
 *
 * A typical game running on PsyZ is structured as follows:
 *    Game ->
 *        PSY-Q decompiled APIs ->
 *            PsyZ platform agnostic PS1 hardware emulation ->
 *                PsyZ platform-specific subsystems ->
 *                    Hardware, Driver or Operating System
 *
 * This umbrella header pulls in every public PsyZ subsystem header. Include it
 * for convenience, or include the individual `psyz/<subsystem>.h` headers
 * directly to keep translation units lean.
 */

#include <psyz/platform.h>
#include <psyz/types.h>
#include <psyz/assert.h>
#include <psyz/log.h>

#include <psyz/audio.h>
#include <psyz/cd.h>
#include <psyz/dma.h>
#include <psyz/gpu.h>
#include <psyz/gte.h>
#include <psyz/input.h>
#include <psyz/spu.h>
#include <psyz/system.h>
#include <psyz/timers.h>
#include <psyz/video.h>

#endif
