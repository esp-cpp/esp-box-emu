#ifndef PSYZ_SPU_H
#define PSYZ_SPU_H

/**
 * @file spu.h
 * @brief Platform-agnostic PS1 SPU emulation endpoints.
 */

#include <psyz/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PS1 SPU constants (the SPU emulator is platform-agnostic; SDL or any other
 * audio backend pulls 44100 Hz stereo short frames from Psyz_SpuPullSamples).
 */
#define PSYZ_SPU_RAM_SIZE (512 * 1024) /**< must be a power of two */
#define PSYZ_SPU_NUM_VOICES 24         /**< match PS1 voice count */
#define PSYZ_SPU_SAMPLE_RATE 44100     /**< match fixed PS1 sample rate */

/**
 * @brief Initialize SPU emulation state
 *
 * Idempotent; safe to call multiple times.
 */
void Psyz_SpuInit(void);

/**
 * @brief Reset SPU state
 *
 * @param hot When non-zero, RAM contents are preserved across the reset
 *            (mirrors the PSX-Q "hot init" semantics for libspu).
 */
void Psyz_SpuReset(int hot);

/**
 * @brief Write one 16-bit value into the SPU register file
 *
 * Certain registers can trigger a side-effect. Please refer to psxspx docs for
 * SPU reference.
 *
 * @param reg_offset Offset relative to 0x1F801C00; valid range 0x000-0x1FF
 * @param value 16-bit value to write
 */
void Psyz_SpuWrite(unsigned int reg_offset, unsigned short value);

/**
 * @brief Read back one 16-bit value from the SPU register file
 *
 * Internally maps to SPU_RXX.
 *
 * @param reg_offset Offset relative to 0x1F801C00; valid range 0x000-0x1FF
 * @return 16-bit register value
 */
unsigned short Psyz_SpuRead(unsigned int reg_offset);

/**
 * @brief Read back the current SPU transfer address
 *
 * Maps to the xfer_addr register.
 *
 * @return Current transfer address
 */
unsigned int Psyz_SpuGetTransferAddr(void);

/**
 * @brief Set the SPU RAM transfer address
 *
 * The destination offset into the 512KB RAM. Maps to the PSX SPU register
 * 0x1F801DA6 (xfer_addr) divided by 8.
 *
 * @param addr Transfer address
 */
void Psyz_SpuSetTransferAddr(unsigned int addr);

/**
 * @brief Push one 16-bit word into the SPU transfer FIFO
 *
 * Maps to a write of SPU register 0x1F801DA8 (xfer_fifo). Each call deposits
 * the word at the current transfer address in SPU RAM and bumps the transfer
 * address by 2.
 *
 * @param word 16-bit word to push
 */
void Psyz_SpuFifoWrite(unsigned short word);

/**
 * @brief Faster bulk version of Psyz_SpuFifoWrite
 *
 * Bypasses individual writes. Uses xfer_addr as the destination address and
 * updates it at the end of the call.
 *
 * @param src Source buffer
 * @param size Number of bytes to transfer
 */
void Psyz_SpuFifoWriteBulk(const unsigned char* src, unsigned int size);

/**
 * @brief Read bytes from SPU RAM
 *
 * Wraps at 512 KB. Does not affect xfer_addr. Useful for debugging.
 *
 * @param offset Byte offset into SPU RAM
 * @param dst Destination buffer
 * @param size Number of bytes to read
 */
void Psyz_SpuMemRead(unsigned int offset, void* dst, unsigned int size);

/**
 * @brief Write bytes into SPU RAM
 *
 * Wraps at 512 KB. Does not affect xfer_addr. Useful for debugging.
 *
 * @param offset Byte offset into SPU RAM
 * @param src Source buffer
 * @param size Number of bytes to write
 */
void Psyz_SpuMemWrite(unsigned int offset, const void* src, unsigned int size);

/**
 * @brief Direct pointer to the 512 KB SPU RAM
 *
 * For tests and offline rendering.
 *
 * @return Pointer to SPU RAM
 */
unsigned char* Psyz_SpuGetRam(void);

/**
 * @brief Generate stereo 16-bit LE PCM frames into out (interleaved L, R)
 *
 * Internally used by the PsyZ Audio subsystem, it can also be used for offline
 * rendering and unit tests.
 *
 * @param out Output buffer
 * @param num_frames Number of stereo frames to generate
 */
void Psyz_SpuPullSamples(short* out, int num_frames);

#ifdef __cplusplus
}
#endif

#endif
