#ifndef PSYZ_DMA_H
#define PSYZ_DMA_H

/**
 * @file dma.h
 * @brief PS1 DMA controller emulation endpoints.
 */

#include <psyz/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DMA channel identifiers
 */
typedef enum {
    DMA_CHANNEL_MDEC_IN,  /**< MDEC input */
    DMA_CHANNEL_MDEC_OUT, /**< MDEC output */
    DMA_CHANNEL_GPU,      /**< GPU */
    DMA_CHANNEL_CD,       /**< CD-ROM */
    DMA_CHANNEL_SPU,      /**< SPU */
    DMA_CHANNEL_PIO,      /**< PIO (expansion port) */
    DMA_CHANNEL_OTC,      /**< Ordering table clear */
} PsyzDmaChannel;

/**
 * @brief Emulate DMA
 *
 * Emulates registers between 0x1F801080 to 0x1F8010EC. The MMIO pointer is
 * calculated with 0x1F801080 + ch * 0x10 + offset * 4
 *
 * @param ch Channel to write to
 * @param offset can be either 0, 1 or 2
 * @param value to write to; can be a raw pointer
 */
void Psyz_DmaWrite(PsyzDmaChannel ch, unsigned offset, u_long value);

#ifdef __cplusplus
}
#endif

#endif
