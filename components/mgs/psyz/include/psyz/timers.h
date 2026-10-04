#ifndef PSYZ_TIMERS_H
#define PSYZ_TIMERS_H

/**
 * @file timers.h
 * @brief Root counter (timer) emulation endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance the reserved SEQ playback timer
 *
 * Reserved. Used by the audio thread to emulate the timer for SEQ playback in
 * absence of an rcnt interrupt. Always assumes the Audio driver plays at
 * 44100Hz.
 *
 * @param n Number of ticks to advance
 */
void Psyz_RcntAdd(int n);

#ifdef __cplusplus
}
#endif

#endif
