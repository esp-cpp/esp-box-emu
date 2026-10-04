// copied from xenogears-decomp
// https://github.com/ladysilverberg/xenogears-decomp/blob/24a5f7a/include/psyq/libdma.h
#ifndef _LIBDMA_H_
#define _LIBDMA_H_

#define DMA_TIMEOUT (0xF00U)
#define DMA_TRANSFER_SUCCESS 0
#define DMA_TRANSFER_TIMEOUT (-2)
#define DMA_BLOCK_SIZE 64 // in bytes

//----------------------------------------------------------------------------------------------------------------------
// DMA Channel Control Register (CHCR)
//----------------------------------------------------------------------------------------------------------------------
// DMA Channel Control Register (CHCR) - Bit Definitions
// Bit 0: Transfer Direction
#define DMA_CHCR_SHFT_TRANSFER_DIR (0)
#define DMA_CHCR_MASK_TRANSFER_DIR (1 << DMA_CHCR_SHFT_TRANSFER_DIR)
#define DMA_CHCR_TRANSFER_TO_RAM                                               \
    (0 << DMA_CHCR_SHFT_TRANSFER_DIR) // To Main RAM
#define DMA_CHCR_TRANSFER_FROM_RAM                                             \
    (1 << DMA_CHCR_SHFT_TRANSFER_DIR) // From Main RAM

// Bit 1: Memory Address Step
#define DMA_CHCR_SHFT_ADDR_STEP (1)
#define DMA_CHCR_MASK_ADDR_STEP (1 << DMA_CHCR_SHFT_ADDR_STEP)
#define DMA_CHCR_ADDR_FORWARD (0 << DMA_CHCR_SHFT_ADDR_STEP)  // Forward (+4)
#define DMA_CHCR_ADDR_BACKWARD (1 << DMA_CHCR_SHFT_ADDR_STEP) // Backward (-4)

// Bits 2-7: Not used (always zero)

// Bit 8: Chopping Enable
#define DMA_CHCR_SHFT_CHOPPING (8)
#define DMA_CHCR_MASK_CHOPPING (1 << DMA_CHCR_SHFT_CHOPPING)
#define DMA_CHCR_CHOPPING_NORMAL                                               \
    (0 << DMA_CHCR_SHFT_CHOPPING) // Normal transfer
#define DMA_CHCR_CHOPPING_ENABLED                                              \
    (1 << DMA_CHCR_SHFT_CHOPPING) // Run CPU during DMA gaps

// Bits 9-10: SyncMode (Transfer Synchronisation/Mode)
#define DMA_CHCR_SHFT_SYNC_MODE (9)
#define DMA_CHCR_MASK_SYNC_MODE                                                \
    ((1 << DMA_CHCR_SHFT_SYNC_MODE) | (1 << (DMA_CHCR_SHFT_SYNC_MODE + 1)))
#define DMA_CHCR_SYNC_MODE_0                                                   \
    (0 << DMA_CHCR_SHFT_SYNC_MODE) // Start immediately, transfer all at once
#define DMA_CHCR_SYNC_MODE_1                                                   \
    (1 << DMA_CHCR_SHFT_SYNC_MODE) // Sync blocks to DMA requests
#define DMA_CHCR_SYNC_MODE_2 (2 << DMA_CHCR_SHFT_SYNC_MODE) // Linked-List mode
#define DMA_CHCR_SYNC_MODE_3                                                   \
    (3 << DMA_CHCR_SHFT_SYNC_MODE) // Reserved (not used)

// Bits 11-15: Not used (always zero)

// Bits 16-18: Chopping DMA Window Size (1 SHL N words)
#define DMA_CHCR_SHFT_DMA_WINDOW_SIZE (16)
#define DMA_CHCR_MASK_DMA_WINDOW_SIZE                                          \
    ((0x1 << DMA_CHCR_SHFT_DMA_WINDOW_SIZE) |                                  \
     (0x1 << (DMA_CHCR_SHFT_DMA_WINDOW_SIZE + 1)) |                            \
     (0x1 << (DMA_CHCR_SHFT_DMA_WINDOW_SIZE + 2)))

// Bit 19: Not used (always zero)

// Bits 20-22: Chopping CPU Window Size (1 SHL N clks)
#define DMA_CHCR_SHFT_CPU_WINDOW_SIZE (20)
#define DMA_CHCR_MASK_CPU_WINDOW_SIZE                                          \
    ((0x1 << DMA_CHCR_SHFT_CPU_WINDOW_SIZE) |                                  \
     (0x1 << (DMA_CHCR_SHFT_CPU_WINDOW_SIZE + 1)) |                            \
     (0x1 << (DMA_CHCR_SHFT_CPU_WINDOW_SIZE + 2)))

// Bit 23: Not used (always zero)

// Bit 24: Start/Busy
#define DMA_CHCR_SHFT_START_BUSY (24)
#define DMA_CHCR_MASK_START_BUSY (1 << DMA_CHCR_SHFT_START_BUSY)
#define DMA_CHCR_START_BUSY_STOPPED                                            \
    (0 << DMA_CHCR_SHFT_START_BUSY) // Stopped/Completed
#define DMA_CHCR_START_BUSY_ENABLED                                            \
    (1 << DMA_CHCR_SHFT_START_BUSY) // Start/Enable/Busy

// Bits 25-27: Not used (always zero)

// Bit 28: Start/Trigger
#define DMA_CHCR_SHFT_START_TRIGGER (28)
#define DMA_CHCR_MASK_START_TRIGGER (1 << DMA_CHCR_SHFT_START_TRIGGER)
#define DMA_CHCR_START_NORMAL (0 << DMA_CHCR_SHFT_START_TRIGGER) // Normal
#define DMA_CHCR_START_MANUAL                                                  \
    (1 << DMA_CHCR_SHFT_START_TRIGGER) // Manual Start (use for SyncMode=0)

// Bit 29: Unknown - Pause?
#define DMA_CHCR_SHFT_PAUSE (29)
#define DMA_CHCR_MASK_PAUSE (1 << DMA_CHCR_SHFT_PAUSE)
#define DMA_CHCR_PAUSE_NO (0 << DMA_CHCR_SHFT_PAUSE) // No pause
#define DMA_CHCR_PAUSE_YES                                                     \
    (1 << DMA_CHCR_SHFT_PAUSE) // Pause (SyncMode=0 only?)

// Bit 30: Unknown
#define DMA_CHCR_SHFT_UNKNOWN_BIT30 (30)
#define DMA_CHCR_MASK_UNKNOWN_BIT30 (1 << DMA_CHCR_SHFT_UNKNOWN_BIT30)

// Bit 31: Not used (always zero)

// Helper macros for extracting/setting multi-bit fields
#define DMA_CHCR_GET_SYNC_MODE(chcr)                                           \
    (((chcr) & DMA_CHCR_SYNC_MODE_MASK) >> DMA_CHCR_SYNC_MODE_SHIFT)
#define DMA_CHCR_SET_SYNC_MODE(chcr, mode)                                     \
    ((chcr) = ((chcr) & ~DMA_CHCR_SYNC_MODE_MASK) |                            \
              ((mode) << DMA_CHCR_SYNC_MODE_SHIFT))

#define DMA_CHCR_GET_DMA_WINDOW_SIZE(chcr)                                     \
    (((chcr) & DMA_CHCR_DMA_WINDOW_SIZE_MASK) >> DMA_CHCR_DMA_WINDOW_SIZE_SHIFT)
#define DMA_CHCR_SET_DMA_WINDOW_SIZE(chcr, size)                               \
    ((chcr) = ((chcr) & ~DMA_CHCR_DMA_WINDOW_SIZE_MASK) |                      \
              ((size) << DMA_CHCR_DMA_WINDOW_SIZE_SHIFT))

#define DMA_CHCR_GET_CPU_WINDOW_SIZE(chcr)                                     \
    (((chcr) & DMA_CHCR_CPU_WINDOW_SIZE_MASK) >> DMA_CHCR_CPU_WINDOW_SIZE_SHIFT)
#define DMA_CHCR_SET_CPU_WINDOW_SIZE(chcr, size)                               \
    ((chcr) = ((chcr) & ~DMA_CHCR_CPU_WINDOW_SIZE_MASK) |                      \
              ((size) << DMA_CHCR_CPU_WINDOW_SIZE_SHIFT))

#define DMA_CHCR_MDEC_IN                                                       \
    (DMA_CHCR_TRANSFER_FROM_RAM | DMA_CHCR_ADDR_FORWARD |                      \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_1 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_MDEC_OUT                                                      \
    (DMA_CHCR_TRANSFER_TO_RAM | DMA_CHCR_ADDR_FORWARD |                        \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_1 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_GPU_VRAM_READ                                                 \
    (DMA_CHCR_TRANSFER_TO_RAM | DMA_CHCR_ADDR_FORWARD |                        \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_1 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_GPU_VRAM_WRITE                                                \
    (DMA_CHCR_TRANSFER_FROM_RAM | DMA_CHCR_ADDR_FORWARD |                      \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_1 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_GPU_LIST                                                      \
    (DMA_CHCR_TRANSFER_FROM_RAM | DMA_CHCR_ADDR_FORWARD |                      \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_2 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_CDROM_NORMAL                                                  \
    (DMA_CHCR_TRANSFER_TO_RAM | DMA_CHCR_ADDR_FORWARD |                        \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_0 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_MANUAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_CDROM_CHOPPED                                                 \
    (DMA_CHCR_TRANSFER_TO_RAM | DMA_CHCR_ADDR_FORWARD |                        \
     DMA_CHCR_CHOPPING_ENABLED | DMA_CHCR_SYNC_MODE_0 |                        \
     (0x4 << DMA_CHCR_SHFT_CPU_WINDOW_SIZE) | DMA_CHCR_START_BUSY_ENABLED |    \
     DMA_CHCR_START_MANUAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_SPU_WRITE                                                     \
    (DMA_CHCR_TRANSFER_FROM_RAM | DMA_CHCR_ADDR_FORWARD |                      \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_1 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_SPU_READ                                                      \
    (DMA_CHCR_TRANSFER_TO_RAM | DMA_CHCR_ADDR_FORWARD |                        \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_1 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_NORMAL | DMA_CHCR_PAUSE_NO)

#define DMA_CHCR_OTC                                                           \
    (DMA_CHCR_TRANSFER_TO_RAM | DMA_CHCR_ADDR_BACKWARD |                       \
     DMA_CHCR_CHOPPING_NORMAL | DMA_CHCR_SYNC_MODE_0 |                         \
     DMA_CHCR_START_BUSY_ENABLED | DMA_CHCR_START_MANUAL | DMA_CHCR_PAUSE_NO)

//----------------------------------------------------------------------------------------------------------------------
// DMA Priority Control Register (DPCR)
//----------------------------------------------------------------------------------------------------------------------
// DMA Priority Control Register (DPCR) - Bit Definitions

// DMA0 (MDECin) - Bits 0-3
#define DMA_DPCR_MASK_DMA0_PRIORITY ((1 << 0) | (1 << 1) | (1 << 2)) // 0-2
#define DMA_DPCR_MASK_DMA0_ENABLE (1 << 3)                           // 3

// DMA1 (MDECout) - Bits 4-7
#define DMA_DPCR_MASK_DMA1_PRIORITY ((1 << 4) | (1 << 5) | (1 << 6)) // 4-6
#define DMA_DPCR_MASK_DMA1_ENABLE (1 << 7)                           // 7

// DMA2 (GPU) - Bits 8-11
#define DMA_DPCR_MASK_DMA2_PRIORITY ((1 << 8) | (1 << 9) | (1 << 10)) // 8-10
#define DMA_DPCR_MASK_DMA2_ENABLE (1 << 11)                           // 11

// DMA3 (CDROM) - Bits 12-15
#define DMA_DPCR_MASK_DMA3_PRIORITY ((1 << 12) | (1 << 13) | (1 << 14)) // 12-14
#define DMA_DPCR_MASK_DMA3_ENABLE (1 << 15)                             // 15

// DMA4 (SPU) - Bits 16-19
#define DMA_DPCR_MASK_DMA4_PRIORITY ((1 << 16) | (1 << 17) | (1 << 18)) // 16-18
#define DMA_DPCR_MASK_DMA4_ENABLE (1 << 19)                             // 19

// DMA5 (PIO) - Bits 20-23
#define DMA_DPCR_MASK_DMA5_PRIORITY ((1 << 20) | (1 << 21) | (1 << 22)) // 20-22
#define DMA_DPCR_MASK_DMA5_ENABLE (1 << 23)                             // 23

// DMA6 (OTC) - Bits 24-27
#define DMA_DPCR_MASK_DMA6_PRIORITY ((1 << 24) | (1 << 25) | (1 << 26)) // 24-26
#define DMA_DPCR_MASK_DMA6_ENABLE (1 << 27)                             // 27

// Unknown bits - Bits 28-31
#define DMA_DPCR_MASK_UNKNOWN_OFFSET                                           \
    ((1 << 28) | (1 << 29) | (1 << 30))       // 28-30
#define DMA_DPCR_MASK_UNKNOWN_BIT31 (1 << 31) // 31

// Shift amounts for priority fields
#define DMA_DPCR_DMA0_PRIORITY_SHIFT 0
#define DMA_DPCR_DMA1_PRIORITY_SHIFT 4
#define DMA_DPCR_DMA2_PRIORITY_SHIFT 8
#define DMA_DPCR_DMA3_PRIORITY_SHIFT 12
#define DMA_DPCR_DMA4_PRIORITY_SHIFT 16
#define DMA_DPCR_DMA5_PRIORITY_SHIFT 20
#define DMA_DPCR_DMA6_PRIORITY_SHIFT 24
#define DMA_DPCR_UNKNOWN_OFFSET_SHIFT 28

// Priority values (0=Highest, 7=Lowest)
#define DMA_PRIORITY_HIGHEST 0
#define DMA_PRIORITY_HIGH 3
#define DMA_PRIORITY_MEDIUM 5
#define DMA_PRIORITY_LOWEST 7

// Enable/Disable values
#define DMA_DISABLED (0 << 0)
#define DMA_ENABLED (1 << 0)

#define DMA_DPCR_SPU_PRIORITY_HIGH                                             \
    (DMA_PRIORITY_HIGH << DMA_DPCR_DMA4_PRIORITY_SHIFT)
#define DMA_DPCR_SPU_PRIORITY_LOW                                              \
    (DMA_PRIORITY_MEDIUM << DMA_DPCR_DMA4_PRIORITY_SHIFT)

// Default DPCR value from documentation
#define DMA_DPCR_DEFAULT_VALUE 0x07654321

#endif