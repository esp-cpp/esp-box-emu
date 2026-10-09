#ifndef PSYZ_CD_H
#define PSYZ_CD_H

/**
 * @file cd.h
 * @brief CD-ROM drive emulation endpoints.
 */

#include <psyz/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PsyzCdRead {
    unsigned int sector;  /**< disk sector, can be used as a unique file id */
    unsigned int size;    /**< byte count required to be read */
    void* buffer;         /**< buffer to read into, max index is buffer[size-1] */
};

/**
 * @brief Callback invoked when a disk read is triggered
 *
 * @param read Read request describing sector, size and destination buffer
 * @return Bytes read; -1 if not found, unsuccessful or not implemented
 */
typedef int (*PsyzCdReadCB)(struct PsyzCdRead* read);

/**
 * @brief Set path to CUE file, simulating a CD loaded
 *
 * Passing a NULL will unset a previously set disk path.
 *
 * @param diskPath Path to the CUE file, or NULL to unset
 * @return 0 on success, otherwise CUE parsing failed
 */
int Psyz_CdSetDiskPath(const char* diskPath);

/**
 * @brief Simulate CD-ROM drive shell (lid) opening or closing
 *
 * @param is_open Non-zero to open the shell, zero to close it
 */
void Psyz_CdShellOpen(int is_open);

/**
 * @brief Pull PCM audio samples from the CD as interleaved stereo frames
 *
 * In XA mode, ADPCM sectors are decoded before being returned. CdMix
 * attenuation has already been applied to the output. Called internally from
 * libcd, and typically not used by the user.
 *
 * @param out Output buffer
 * @param num_frames Number of stereo frames requested
 * @return Number of frames read
 */
size_t Psyz_CdPullSamples(short* out, size_t num_frames);

/**
 * @brief Set callback invoked when a disk read is triggered
 *
 * If cb is NULL or returns a negative value, PSY-Z falls back to CD emulation.
 *
 * @param cb Disk read callback, or NULL to clear
 */
void Psyz_CdSetReadCB(PsyzCdReadCB cb);

#ifdef __cplusplus
}
#endif

#endif
