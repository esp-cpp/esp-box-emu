#ifndef PSYZ_AUDIO_H
#define PSYZ_AUDIO_H

/**
 * @file audio.h
 * @brief Host audio subsystem backend endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the audio subsystem
 *
 * Opens the host audio device and the SPU mixer. Idempotent.
 *
 * @return 0 on success, -1 on failure
 */
int Psyz_AudioInit(void);

/**
 * @brief Shut down the audio subsystem and release its resources
 *
 * Typically used to undo the audio subsystem init for audio offline rendering.
 */
void Psyz_AudioDestroy(void);

/**
 * @brief Pause the host audio device so it stops pulling samples from the SPU
 *
 * Psyz_AudioUnpause must be called to resume.
 */
void Psyz_AudioPause(void);

/**
 * @brief Resume audio playback after Psyz_AudioPause
 */
void Psyz_AudioUnpause(void);

/**
 * @brief Acquire the audio mutex
 *
 * Intended for tests and offline rendering that need to suspend the SDL audio
 * callback while pulling samples directly. Must be paired with
 * Psyz_AudioUnlock.
 */
void Psyz_AudioLock(void);

/**
 * @brief Release the audio mutex acquired with Psyz_AudioLock
 */
void Psyz_AudioUnlock(void);

#ifdef __cplusplus
}
#endif

#endif
