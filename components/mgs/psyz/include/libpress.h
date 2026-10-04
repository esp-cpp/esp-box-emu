#ifndef LIBPRESS_H
#define LIBPRESS_H
#include <psyz/types.h>

/**
 * @file libpress.h
 * @brief Data Compression Library
 *
 * This library provides hardware-accelerated data decompression using the
 * MDEC (Motion Decoder) chip. It supports JPEG-like DCT decompression for
 * images and video, as well as ADPCM audio compression.
 *
 * Key features:
 * - DCT (Discrete Cosine Transform) decompression
 * - MDEC hardware acceleration
 * - Asynchronous decompression with callbacks
 * - VLC (Variable Length Code) decoding
 * - SPU ADPCM encoding
 * - Environment management for compression parameters
 */

/* SPU Encoding Constants */
#define ENCSPU_ENCODE_LOOP 1           /**< Generate loop waveform data */
#define ENCSPU_ENCODE_NO_LOOP 0        /**< Generate non-loop waveform data */
#define ENCSPU_ENCODE_ENDIAN_BIG 1     /**< 16-bit big endian */
#define ENCSPU_ENCODE_ENDIAN_LITTLE 0  /**< 16-bit little endian */
#define ENCSPU_ENCODE_WHOLE 0          /**< Whole encoding */
#define ENCSPU_ENCODE_START 1          /**< Start divided encoding */
#define ENCSPU_ENCODE_CONTINUE 2       /**< Continue divided encoding */
#define ENCSPU_ENCODE_END 3            /**< End divided encoding */
#define ENCSPU_ENCODE_MIDDLE_QUALITY 0 /**< Middle quality (faster) */
#define ENCSPU_ENCODE_HIGH_QUALITY 1   /**< High quality (slower) */
#define ENC_ENCODE_ERROR (-1)          /**< Encoding error */

typedef void (*DecDCCb)(); /**< Callback */

/* Structures */

/**
 * @brief Quantization tables used during MDEC decoding process
 *
 * Contains the tables used during the reverse-quantization step of the MDEC
 * decoding process. The values are sorted in diagonal zig-zag scanning order.
 */
typedef struct {
    u_char iq_y[64]; /**< Brightness component quantization table */
    u_char iq_c[64]; /**< Chrominance component quantization table */
    short dct[64];   /**< System reserved */
} DECDCTENV;

/**
 * @brief SPU encode environment attribute structure
 *
 * Specifies the SPU encode environment attributes for EncSPU() and EncSPU2()
 * functions.
 */
typedef struct {
    short* src;     /**< 16-bit PCM data address */
    short* dest;    /**< PlayStation original waveform data */
    short* work;    /**< Work area when encode processing (168 bytes) */
    int size;       /**< 16-bit PCM data size (in bytes) */
    int loop_start; /**< PCM data loop start point (in bytes) */
    char loop;      /**< Loop waveform generation (ENCSPU_ENCODE_[NO_]LOOP) */
    char byte_swap; /**< PCM endian (ENCSPU_ENCODE_ENDIAN_BIG/LITTLE) */
    char proceed;   /**< Whole/Divided encoding (ENCSPU_ENCODE_*) */
    char quality;   /**< Encoding quality (EncSPU2 only) */
    char pad4;      /**< System reserved */
} ENCSPUENV;

/**
 * @brief VLC table type
 *
 * VLC (Variable Length Code) decoding table. Size can be obtained with
 * sizeof(DECDCTTAB). The table is held in compressed format (4KB) and
 * decompressed to 64KB when used.
 */
typedef u_short DECDCTTAB[0x2000];

/* Functions */

/**
 * @brief Get size of run-level DCT data
 *
 * Returns the uncompressed length of the data contained in the
 * Huffman-encoded bitstream. Does not perform actual decoding.
 *
 * When using DecDCTvlc()/DecDCTvlc2(), reserve a 1-word header buffer in
 * addition to the size returned.
 *
 * @param bs Pointer to bitstream
 * @return Length of uncompressed data in long words
 */
int DecDCTBufSize(u_long* bs);

/**
 * @brief Get current quantization tables and environment
 *
 * Returns the current decoding environment to env.
 *
 * @param env Pointer to decoding environment
 * @return Address of env
 */
DECDCTENV* DecDCTGetEnv(DECDCTENV* env);

/**
 * @brief Begin decoding RLE-encoded MDEC image data
 *
 * Begins decoding RLE-encoded MDEC image data. Maximum 128k may be decoded
 * at a time. Resulting image data is retrieved by DecDCTout().
 *
 * mode bit 0: 0=16-bit direct color, 1=24-bit direct color
 * mode bit 1: STP bit for bit 15 of pixel (16-bit mode only)
 *
 * Non-blocking. Use DecDCTinSync() or DecDCTinCallback() to detect
 * completion. If called before previous operation completes, blocks until
 * previous operation finishes.
 *
 * @param runlevel Pointer to input runlevel
 * @param mode Decode mode (bit 0: depth, bit 1: STP)
 */
void DecDCTin(u_long* runlevel, int mode);

/**
 * @brief Install callback for MDEC transmission termination
 *
 * Installs user-defined callback routine called when DecDCTin() transmission
 * completes. If func is 0, callback is disabled.
 *
 * Callback is called during interrupt but is not an interrupt handler. Should
 * return as soon as possible.
 *
 * @param func Pointer to callback function (0 to disable)
 * @return Pointer to previously set callback function
 */
DecDCCb DecDCTinCallback(DecDCCb func);

/**
 * @brief Detect DecDCTin() termination
 *
 * Detects termination of DecDCTin(). Synchronization must be performed after
 * reading appropriate amount of data with DecDCTout(). Calling without using
 * DecDCTout() after DecDCTin() causes timeout and MDEC reset.
 *
 * @param mode 0: blocks until termination, 1: status notification only
 * @return 1 if transmission in process, 0 if not being performed
 */
int DecDCTinSync(int mode);

/**
 * @brief Receive decoded data from image processing subsystem
 *
 * Decoded RLE-encoded MDEC image data is stored in buffer. Data is output
 * one 16x16 macroblock at a time. size must be multiple of macroblock size:
 * 128 words for 16-bit, 192 words for 24-bit.
 *
 * Non-blocking. Use DecDCToutSync() or DecDCToutCallback() to detect
 * completion. If called before previous operation completes, blocks until
 * previous operation finishes.
 *
 * @param cell Pointer to decoded image data buffer
 * @param size Received data size in int words
 */
void DecDCTout(u_long* cell, int size);

/**
 * @brief Install callback for MDEC reception termination
 *
 * Installs user-defined callback routine called when DecDCTout() transmission
 * completes. If func is 0, callback is disabled.
 *
 * Callback is called during interrupt but is not an interrupt handler. Should
 * return as soon as possible.
 *
 * @param func Pointer to callback function (0 to disable)
 * @return Pointer to previously set callback function
 */
DecDCCb DecDCToutCallback(DecDCCb func);

/**
 * @brief Detect termination of DecDCTout()
 *
 * Detects termination of DecDCTout().
 *
 * @param mode 0: blocks until termination, 1: status notification only
 * @return 1 if reception in progress, 0 if not being performed
 */
int DecDCToutSync(int mode);

/**
 * @brief Set image-processing-subsystem environment
 *
 * Sets the quantization tables and environment data used during the
 * reverse-quantization step of the MDEC decoding process.
 *
 * @param env Pointer to decoding environment
 * @return Address of env
 */
DECDCTENV* DecDCTPutEnv(DECDCTENV* env);

/**
 * @brief Initialize image processing subsystem
 *
 * Resets the image processing subsystem.
 *
 * mode 0: Initializes all internal states (longer processing time)
 * mode 1: Discontinues only current decoding (faster)
 *
 * @param mode Reset mode (0: full reset, 1: abort current only)
 */
void DecDCTReset(int mode);

/**
 * @brief Decode Huffman-compressed MDEC image data
 *
 * Builds run-level intermediate format in buf by decoding bitstream bs.
 * If runlevel data exceeds DecDCTvlcSize(), function is interrupted and
 * returns control. Restart by executing DecDCTvlc(0, 0).
 *
 * buf must reserve 1 word area added to header buffer from DecDCTBufSize().
 *
 * Blocking function. This is the first stage of decoding; bitstream must
 * always be decoded using DecDCTvlc() before DecDCTin() is executed.
 * Partial result runlevel cannot be provided as DecDCTin() input.
 *
 * @param bs Input bitstream (0 to continue interrupted decode)
 * @param buf Output runlevel buffer (0 to continue interrupted decode)
 * @return 0: all bitstream decoded, 1: bitstream left non-decoded, -1: failed
 */
int DecDCTvlc(u_long* bs, u_long* buf);

/**
 * @brief Decode VLC with custom table
 *
 * Builds run-level intermediate format in buf by decoding bitstream bs using
 * table. When runlevel exceeds DecDCTvlcSize2(), function is suspended.
 * Restart by executing DecDCTvlc2(0, 0, table).
 *
 * buf must reserve 1 word area added to header buffer from DecDCTBufSize().
 *
 * Blocking function. VLC table should be decoded in advance using
 * DecDCTvlcBuild(). Partial result runlevel cannot be provided as DecDCTin()
 * input.
 *
 * @param bs Input bitstream (0 to continue suspended decode)
 * @param buf Output runlevel buffer (0 to continue suspended decode)
 * @param table VLC table
 * @return 0: all bitstream decoded, 1: bitstream left non-decoded, -1: failed
 */
int DecDCTvlc2(u_long* bs, u_long* buf, DECDCTTAB table);

/**
 * @brief Build the VLC table
 *
 * Builds the VLC table for use with DecDCTvlc2(). The VLC table is held in
 * compressed format (4KB) and decompressed to 64KB when used.
 *
 * @param table VLC buffer (size: sizeof(DECDCTTAB))
 */
void DecDCTvlcBuild(u_short* table);

/**
 * @brief Set maximum amount of data returned by DecDCTvlc()
 *
 * Sets the maximum number of int words that DecDCTvlc() can return.
 * Subsequent calls halt after decoding size int words. If size is zero,
 * DecDCTvlc() decodes entire bitstream regardless of length.
 *
 * Allows multiple calls to decode bitstream in chunks using smaller buffer.
 *
 * Blocking function. Bitstream must be converted to run-levels before
 * executing DecDCTin().
 *
 * @param size Maximum decoded runlevel in int words (0: unlimited)
 * @return Previously set buffer size
 */
int DecDCTvlcSize(int size);

/**
 * @brief Set maximum size of single VLC decoding process
 *
 * Sets the maximum size of bitstream that can be decoded per process.
 * DecDCTvlc2() suspends decoding when decoding first block after number of
 * words specified. If size is 0 (default), decoding is not suspended.
 *
 * Blocking function. Bitstream must be converted to run-level before
 * executing DecDCTin().
 *
 * @param size Maximum decoded runlevel in int words (0: unlimited)
 * @return Maximum run level set immediately before
 */
int DecDCTvlcSize2(int size);

/**
 * @brief Encode 16-bit PCM data into PlayStation waveform format
 *
 * Encodes 16-bit straight PCM data into PlayStation waveform data (VAG,
 * without header). Returns encoded data in es_env.dest.
 *
 * PCM data size in es_env.size is in bytes. For looping waveforms, set
 * es_env.loop to ENCSPU_ENCODE_LOOP and es_env.loop_start to loop start
 * point in bytes (rounded down to multiple of 56).
 *
 * For non-looping, set es_env.loop to ENCSPU_ENCODE_NO_LOOP.
 *
 * Set es_env.byte_swap for endian: ENCSPU_ENCODE_ENDIAN_BIG or
 * ENCSPU_ENCODE_ENDIAN_LITTLE.
 *
 * Set es_env.proceed for encoding type: ENCSPU_ENCODE_WHOLE,
 * ENCSPU_ENCODE_START, ENCSPU_ENCODE_CONTINUE, or ENCSPU_ENCODE_END.
 *
 * For scratchpad use, set es_env.work to scratchpad address (168 bytes).
 * If NULL, automatic variables are used.
 *
 * @param es_env SPU encode environment attribute structure
 * @return Encoded waveform data size (VAG), or ENC_ENCODE_ERROR on error
 */
int EncSPU(ENCSPUENV* es_env);

/**
 * @brief Encode 16-bit PCM (high quality version)
 *
 * High-quality version of EncSPU(). Same parameters, but supports
 * es_env.quality setting: ENCSPU_ENCODE_MIDDLE_QUALITY (faster) or
 * ENCSPU_ENCODE_HIGH_QUALITY (slower, better quality).
 *
 * When quality is ENCSPU_ENCODE_HIGH_QUALITY, es_env.work must be NULL.
 *
 * @param es_env SPU encode environment attribute structure
 * @return Encoded waveform data size (VAG), or ENC_ENCODE_ERROR on error
 */
int EncSPU2(ENCSPUENV* es_env);

#endif /* LIBPRESS_H */
