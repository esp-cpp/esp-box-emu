#ifndef LIBSPU_H
#define LIBSPU_H
#include <psyz/types.h>

/**
 * @file libspu.h
 * @brief Basic Sound Library (libspu)
 *
 * This library provides low-level functions for controlling the PlayStation
 * Sound Processing Unit (SPU), including voice management, ADSR envelope
 * control, reverb effects, memory management, and data transfer.
 */

/*============================================================================
 * Voice Channel Constants
 *============================================================================*/

#define SPU_00CH (1 << 0)
#define SPU_01CH (1 << 1)
#define SPU_02CH (1 << 2)
#define SPU_03CH (1 << 3)
#define SPU_04CH (1 << 4)
#define SPU_05CH (1 << 5)
#define SPU_06CH (1 << 6)
#define SPU_07CH (1 << 7)
#define SPU_08CH (1 << 8)
#define SPU_09CH (1 << 9)
#define SPU_10CH (1 << 10)
#define SPU_11CH (1 << 11)
#define SPU_12CH (1 << 12)
#define SPU_13CH (1 << 13)
#define SPU_14CH (1 << 14)
#define SPU_15CH (1 << 15)
#define SPU_16CH (1 << 16)
#define SPU_17CH (1 << 17)
#define SPU_18CH (1 << 18)
#define SPU_19CH (1 << 19)
#define SPU_20CH (1 << 20)
#define SPU_21CH (1 << 21)
#define SPU_22CH (1 << 22)
#define SPU_23CH (1 << 23)

#define SPU_0CH SPU_00CH
#define SPU_1CH SPU_01CH
#define SPU_2CH SPU_02CH
#define SPU_3CH SPU_03CH
#define SPU_4CH SPU_04CH
#define SPU_5CH SPU_05CH
#define SPU_6CH SPU_06CH
#define SPU_7CH SPU_07CH
#define SPU_8CH SPU_08CH
#define SPU_9CH SPU_09CH

#define SPU_ALLCH 0x00FFFFFF /**< All 24 channels */

/*============================================================================
 * Voice Attribute Mask Constants
 *============================================================================*/

#define SPU_VOICE_VOLL (1 << 0)        /**< Volume (left) */
#define SPU_VOICE_VOLR (1 << 1)        /**< Volume (right) */
#define SPU_VOICE_VOLMODEL (1 << 2)    /**< Volume mode (left) */
#define SPU_VOICE_VOLMODER (1 << 3)    /**< Volume mode (right) */
#define SPU_VOICE_PITCH (1 << 4)       /**< Tone (pitch setting) */
#define SPU_VOICE_NOTE (1 << 5)        /**< Tone (note setting) */
#define SPU_VOICE_SAMPLE_NOTE (1 << 6) /**< Waveform data sample note */
#define SPU_VOICE_WDSA (1 << 7)        /**< Waveform data start address */
#define SPU_VOICE_ADSR_AMODE (1 << 8)  /**< ADSR attack rate mode */
#define SPU_VOICE_ADSR_SMODE (1 << 9)  /**< ADSR sustain rate mode */
#define SPU_VOICE_ADSR_RMODE (1 << 10) /**< ADSR release rate mode */
#define SPU_VOICE_ADSR_AR (1 << 11)    /**< ADSR attack rate */
#define SPU_VOICE_ADSR_DR (1 << 12)    /**< ADSR decay rate */
#define SPU_VOICE_ADSR_SR (1 << 13)    /**< ADSR sustain rate */
#define SPU_VOICE_ADSR_RR (1 << 14)    /**< ADSR release rate */
#define SPU_VOICE_ADSR_SL (1 << 15)    /**< ADSR sustain level */
#define SPU_VOICE_LSAX (1 << 16)       /**< Loop start address */
#define SPU_VOICE_ADSR_ADSR1 (1 << 17) /**< ADSR adsr1 for VagAtr */
#define SPU_VOICE_ADSR_ADSR2 (1 << 18) /**< ADSR adsr2 for VagAtr */

/*============================================================================
 * Volume Mode Constants
 *============================================================================*/

#define SPU_VOICE_DIRECT 0     /**< Direct mode */
#define SPU_VOICE_LINEARIncN 1 /**< Linear increase (normal phase) */
#define SPU_VOICE_LINEARIncR 2 /**< Linear increase (inverted phase) */
#define SPU_VOICE_LINEARDecN 3 /**< Linear decrease (normal phase) */
#define SPU_VOICE_LINEARDecR 4 /**< Linear decrease (inverted phase) */
#define SPU_VOICE_EXPIncN 5    /**< Exponential increase (normal phase) */
#define SPU_VOICE_EXPIncR 6    /**< Exponential increase (inverted phase) */
#define SPU_VOICE_EXPDec 7     /**< Exponential decrease */

/*============================================================================
 * Reverb Attribute Mask Constants
 *============================================================================*/

#define SPU_REV_MODE (1 << 0)      /**< Mode setting */
#define SPU_REV_DEPTHL (1 << 1)    /**< Reverb depth (left) */
#define SPU_REV_DEPTHR (1 << 2)    /**< Reverb depth (right) */
#define SPU_REV_DELAYTIME (1 << 3) /**< Delay time (ECHO, DELAY only) */
#define SPU_REV_FEEDBACK (1 << 4)  /**< Feedback (ECHO, DELAY only) */

/*============================================================================
 * Reverb Mode Constants
 *============================================================================*/

#define SPU_REV_MODE_OFF 0          /**< Reverb off */
#define SPU_REV_MODE_ROOM 1         /**< Room reverb */
#define SPU_REV_MODE_STUDIO_A 2     /**< Studio (small) */
#define SPU_REV_MODE_STUDIO_B 3     /**< Studio (medium) */
#define SPU_REV_MODE_STUDIO_C 4     /**< Studio (large) */
#define SPU_REV_MODE_HALL 5         /**< Hall */
#define SPU_REV_MODE_SPACE 6        /**< Space echo */
#define SPU_REV_MODE_ECHO 7         /**< Echo */
#define SPU_REV_MODE_DELAY 8        /**< Delay */
#define SPU_REV_MODE_PIPE 9         /**< Half echo (pipe) */
#define SPU_REV_MODE_MAX 10         /**< Maximum mode value */
#define SPU_REV_MODE_CLEAR_WA 0x100 /**< Clear work area flag */

/*============================================================================
 * Common Attribute Mask Constants
 *============================================================================*/

#define SPU_COMMON_MVOLL (1 << 0)     /**< Master volume (left) */
#define SPU_COMMON_MVOLR (1 << 1)     /**< Master volume (right) */
#define SPU_COMMON_MVOLMODEL (1 << 2) /**< Master volume mode (left) */
#define SPU_COMMON_MVOLMODER (1 << 3) /**< Master volume mode (right) */
#define SPU_COMMON_RVOLL (1 << 4)     /**< Reverb volume mode (left) */
#define SPU_COMMON_RVOLR (1 << 5)     /**< Reverb volume mode (right) */
#define SPU_COMMON_CDVOLL (1 << 6)    /**< CD input volume (left) */
#define SPU_COMMON_CDVOLR (1 << 7)    /**< CD input volume (right) */
#define SPU_COMMON_CDREV (1 << 8)     /**< CD input reverb ON/OFF */
#define SPU_COMMON_CDMIX (1 << 9)     /**< CD input ON/OFF */
#define SPU_COMMON_EXTVOLL (1 << 10)  /**< External input volume (left) */
#define SPU_COMMON_EXTVOLR (1 << 11)  /**< External input volume (right) */
#define SPU_COMMON_EXTREV (1 << 12)   /**< External input reverb ON/OFF */
#define SPU_COMMON_EXTMIX (1 << 13)   /**< External input ON/OFF */

/*============================================================================
 * General Constants
 *============================================================================*/

#define SPU_OFF 0   /**< Off state */
#define SPU_ON 1    /**< On state */
#define SPU_BIT 2   /**< Direct bit pattern mode */
#define SPU_RESET 3 /**< Reset state */

#define SPU_ERROR (-1) /**< Error return value */

#define SPU_MALLOC_RECSIZ 8 /**< Memory management record size */

/*============================================================================
 * Transfer Mode Constants
 *============================================================================*/

#define SPU_TRANSFER_BY_DMA 0 /**< DMA transfer */
#define SPU_TRANSFER_BY_IO 1  /**< I/O transfer */

/*============================================================================
 * Transfer Status Constants
 *============================================================================*/

#define SPU_TRANSFER_WAIT 1   /**< Wait until transfer ends */
#define SPU_TRANSFER_PEEK 0   /**< Check if transfer ended */
#define SPU_TRANSFER_GLANCE 0 /**< Same as PEEK */

/*============================================================================
 * Key Status Constants
 *============================================================================*/

#define SPU_ON_ENV_OFF 2 /**< Key on, envelope 0 */
#define SPU_OFF_ENV_ON 3 /**< Key off, envelope not 0 */

/*============================================================================
 * Decoded Data Constants
 *============================================================================*/

#define SPU_DECODEDATA_SIZE 0x200 /**< Decode buffer size (shorts) */
#define SPU_DECODE_FIRSTHALF 0    /**< First half of decode buffer */
#define SPU_DECODE_SECONDHALF 1   /**< Second half of decode buffer */

/*============================================================================
 * Decode Data Flags
 *============================================================================*/

#define SPU_CDONLY 0    /**< Transfer only CD input */
#define SPU_VOICEONLY 1 /**< Transfer only Voice 1, 3 */
#define SPU_ALL 2       /**< Transfer all data */

/*============================================================================
 * Environment Constants
 *============================================================================*/

#define SPU_ENV_EVENT_QUEUEING (1 << 0) /**< Event queueing attribute */

/*============================================================================
 * Event Constants
 *============================================================================*/

#define SPU_EVENT_KEY (1 << 0)      /**< Key ON/OFF event */
#define SPU_EVENT_PITCHLFO (1 << 1) /**< Pitch LFO voice set event */
#define SPU_EVENT_NOISE (1 << 2)    /**< Noise voice set event */
#define SPU_EVENT_REVERB (1 << 3)   /**< Reverb voice set event */
#define SPU_EVENT_ALL 0x0F          /**< All events */

/*============================================================================
 * Reverb Work Area Check Constants
 *============================================================================*/

#define SPU_CHECK 0 /**< Check current status */
#define SPU_DIAG 1  /**< Diagnostic check */

/*============================================================================
 * SPU Streaming Constants
 *============================================================================*/

#define SPU_ST_NOT_AVAILABLE (-1) /**< Streaming not available */
#define SPU_ST_IDLE 0             /**< Idle state */
#define SPU_ST_PREPARE 1          /**< Preparation state */
#define SPU_ST_TRANSFER 2         /**< Transfer state */
#define SPU_ST_FINAL 3            /**< Final state */
#define SPU_ST_PLAY 4             /**< Playing state */

#define SPU_ST_ACCEPT 0               /**< Request accepted */
#define SPU_ST_WRONG_STATUS (-1)      /**< Wrong status */
#define SPU_ST_INVALID_ARGUMENTS (-2) /**< Invalid arguments */

#define SPU_SUCCESS 0         /**< Success */
#define SPU_INVALID_ARGS (-1) /**< Invalid arguments */

/*============================================================================
 * Callback Types
 *============================================================================*/

/** @brief SPU IRQ callback function type */
typedef void (*SpuIRQCallbackProc)(void);

/** @brief SPU transfer callback function type */
typedef void (*SpuTransferCallbackProc)(void);

/** @brief SPU streaming callback function type */
typedef void (*SpuStCallbackProc)(u_long voice_bit, long status);

/*============================================================================
 * Structure Definitions
 *============================================================================*/

/**
 * @brief Volume structure
 *
 * Used for L/R channel volume values.
 */
typedef struct SpuVolume {
    short left;  /**< Left channel value */
    short right; /**< Right channel value */
} SpuVolume;

/**
 * @brief Voice attributes
 *
 * Used when setting/checking the attributes of each voice.
 */
typedef struct SpuVoiceAttr {
    u_long voice;        /**< Voice bit value (SPU_0CH..SPU_23CH) */
    u_long mask;         /**< Attributes to set (bit string) */
    SpuVolume volume;    /**< Volume */
    SpuVolume volmode;   /**< Volume mode */
    SpuVolume volumex;   /**< Current volume (read-only) */
    u_short pitch;       /**< Interval (pitch specification) */
    u_short note;        /**< Interval (note specification) */
    u_short sample_note; /**< Waveform data sample note */
    short envx;          /**< Current envelope value (read-only) */
    u_long addr;         /**< Waveform data start address */
    u_long loop_addr;    /**< Loop start address */
    int a_mode;          /**< Attack rate mode */
    int s_mode;          /**< Sustain rate mode */
    int r_mode;          /**< Release rate mode */
    u_short ar;          /**< Attack rate (0x00-0x7f) */
    u_short dr;          /**< Decay rate (0x0-0xf) */
    u_short sr;          /**< Sustain rate (0x00-0x7f) */
    u_short rr;          /**< Release rate (0x00-0x1f) */
    u_short sl;          /**< Sustain level (0x0-0xf) */
    u_short adsr1;       /**< ADSR1 for VagAtr */
    u_short adsr2;       /**< ADSR2 for VagAtr */
} SpuVoiceAttr;

/**
 * @brief Voice attributes with voice number
 *
 * Specifies voice attributes for a specific voice.
 */
typedef struct SpuLVoiceAttr {
    short voiceNum;    /**< Voice number (0-23) */
    short pad;         /**< Padding */
    SpuVoiceAttr attr; /**< Voice attributes */
} SpuLVoiceAttr;

/**
 * @brief External input attributes
 *
 * Used for CD and external digital input.
 */
typedef struct SpuExtAttr {
    SpuVolume volume; /**< Volume */
    int reverb;       /**< Reverb on/off */
    int mix;          /**< Mixing on/off */
} SpuExtAttr;

/**
 * @brief Common attributes
 *
 * Used when setting/checking attributes common to all voices.
 */
typedef struct SpuCommonAttr {
    u_long mask;        /**< Set mask */
    SpuVolume mvol;     /**< Master volume */
    SpuVolume mvolmode; /**< Master volume mode */
    SpuVolume mvolx;    /**< Current master volume */
    SpuExtAttr cd;      /**< CD input attributes */
    SpuExtAttr ext;     /**< External digital input attributes */
} SpuCommonAttr;

/**
 * @brief Reverb attributes
 *
 * Used when setting/checking reverb parameters.
 */
typedef struct SpuReverbAttr {
    u_long mask;     /**< Set mask */
    int mode;        /**< Reverb mode */
    SpuVolume depth; /**< Reverb depth */
    int delay;       /**< Delay time (ECHO, DELAY only) */
    int feedback;    /**< Feedback (ECHO, DELAY only) */
} SpuReverbAttr;

/**
 * @brief SPU environment attributes
 *
 * Used to set the basic sound library environment.
 */
typedef struct SpuEnv {
    u_long mask;     /**< Setting mask */
    u_long queueing; /**< Event queueing */
} SpuEnv;

/**
 * @brief Decoded data from SPU
 *
 * Structure for getting CD-ROM and voice data decoded by SPU.
 */
typedef struct SpuDecodeData {
    short cd_left[SPU_DECODEDATA_SIZE];  /**< CD L channel data */
    short cd_right[SPU_DECODEDATA_SIZE]; /**< CD R channel data */
    short voice1[SPU_DECODEDATA_SIZE];   /**< Voice 1 data */
    short voice3[SPU_DECODEDATA_SIZE];   /**< Voice 3 data */
} SpuDecodeData;

/**
 * @brief SPU streaming voice attributes
 *
 * Contains attributes for each stream.
 */
typedef struct SpuStVoiceAttr {
    char status;           /**< Stream status */
    char pad1, pad2, pad3; /**< Padding */
    int last_size;         /**< Size of final data transfer */
    u_long buf_addr;       /**< Start address of stream buffer */
    u_long data_addr;      /**< Start address of stream data in main RAM */
} SpuStVoiceAttr;

/**
 * @brief SPU streaming environment attributes
 *
 * Used for SPU streaming environment settings.
 */
typedef struct SpuStEnv {
    int size;                 /**< Stream buffer size */
    int low_priority;         /**< Priority setting */
    SpuStVoiceAttr voice[24]; /**< Each stream attribute set */
} SpuStEnv;

/*============================================================================
 * Initialization Functions
 *============================================================================*/

/**
 * @brief Initialize SPU
 *
 * Initializes the SPU. Called only once within the program.
 */
void SpuInit(void);

/**
 * @brief Initialize SPU (hot reset)
 *
 * Initializes SPU while preserving sound buffer status.
 */
void SpuInitHot(void);

/**
 * @brief Start SPU processing
 *
 * Starts SPU processing. Called by SpuInit(), so not needed at init.
 */
void SpuStart(void);

/**
 * @brief Terminate SPU processing
 *
 * Terminates SPU processing and releases events.
 */
void SpuQuit(void);

/*============================================================================
 * Voice Attribute Functions
 *============================================================================*/

/**
 * @brief Set attributes for voices
 *
 * @param attr Pointer to voice attributes
 */
void SpuSetVoiceAttr(SpuVoiceAttr* attr);

/**
 * @brief Get voice attributes
 *
 * @param attr Pointer to voice attributes
 */
void SpuGetVoiceAttr(SpuVoiceAttr* attr);

/**
 * @brief Set attributes for a specific voice
 *
 * @param voiceNum Voice number (0-23)
 * @param attr Voice attributes
 */
void SpuNSetVoiceAttr(int voiceNum, SpuVoiceAttr* attr);

/**
 * @brief Get attributes for a specific voice
 *
 * @param voiceNum Voice number (0-23)
 * @param attr Voice attributes
 */
void SpuNGetVoiceAttr(int voiceNum, SpuVoiceAttr* attr);

/**
 * @brief Set attributes for a range of voices
 *
 * @param min Lower limit of voice number
 * @param max Upper limit of voice number
 * @param attr Voice attributes
 * @return SPU_SUCCESS or SPU_INVALID_ARGS
 */
long SpuRSetVoiceAttr(long min, long max, SpuVoiceAttr* attr);

/**
 * @brief Set attributes for multiple voices
 *
 * @param num Number of elements in argList
 * @param argList Array of voice attributes
 */
void SpuLSetVoiceAttr(int num, SpuLVoiceAttr* argList);

/*============================================================================
 * Voice Volume Functions
 *============================================================================*/

/**
 * @brief Set voice volume
 *
 * @param voiceNum Voice number (0-23)
 * @param volumeL Left volume
 * @param volumeR Right volume
 */
void SpuSetVoiceVolume(int voiceNum, short volumeL, short volumeR);

/**
 * @brief Get voice volume
 *
 * @param voiceNum Voice number (0-23)
 * @param volumeL Pointer to left volume
 * @param volumeR Pointer to right volume
 */
void SpuGetVoiceVolume(int voiceNum, short* volumeL, short* volumeR);

/**
 * @brief Set voice volume and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param volumeL Left volume
 * @param volumeR Right volume
 * @param volModeL Left volume mode
 * @param volModeR Right volume mode
 */
void SpuSetVoiceVolumeAttr(
    int voiceNum, short volumeL, short volumeR, short volModeL, short volModeR);

/**
 * @brief Get voice volume and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param volumeL Pointer to left volume
 * @param volumeR Pointer to right volume
 * @param volModeL Pointer to left volume mode
 * @param volModeR Pointer to right volume mode
 */
void SpuGetVoiceVolumeAttr(int voiceNum, short* volumeL, short* volumeR,
                           short* volModeL, short* volModeR);

/**
 * @brief Get current voice volume
 *
 * @param voiceNum Voice number (0-23)
 * @param volumeXL Pointer to current left volume
 * @param volumeXR Pointer to current right volume
 */
void SpuGetVoiceVolumeX(int voiceNum, short* volumeXL, short* volumeXR);

/*============================================================================
 * Voice Pitch Functions
 *============================================================================*/

/**
 * @brief Set voice pitch
 *
 * @param voiceNum Voice number (0-23)
 * @param pitch Pitch value (0x0000-0x3fff)
 */
void SpuSetVoicePitch(int voiceNum, u_short pitch);

/**
 * @brief Get voice pitch
 *
 * @param voiceNum Voice number (0-23)
 * @param pitch Pointer to pitch value
 */
void SpuGetVoicePitch(int voiceNum, u_short* pitch);

/**
 * @brief Set voice note
 *
 * @param voiceNum Voice number (0-23)
 * @param note Note value
 */
void SpuSetVoiceNote(int voiceNum, u_short note);

/**
 * @brief Get voice note
 *
 * @param voiceNum Voice number (0-23)
 * @param note Pointer to note value
 */
void SpuGetVoiceNote(int voiceNum, u_short* note);

/**
 * @brief Set voice sample note
 *
 * @param voiceNum Voice number (0-23)
 * @param sampleNote Sample note value
 */
void SpuSetVoiceSampleNote(int voiceNum, u_short sampleNote);

/**
 * @brief Get voice sample note
 *
 * @param voiceNum Voice number (0-23)
 * @param sampleNote Pointer to sample note value
 */
void SpuGetVoiceSampleNote(int voiceNum, u_short* sampleNote);

/*============================================================================
 * Voice Address Functions
 *============================================================================*/

/**
 * @brief Set waveform data start address
 *
 * @param voiceNum Voice number (0-23)
 * @param startAddr Start address
 */
void SpuSetVoiceStartAddr(int voiceNum, u_long startAddr);

/**
 * @brief Get waveform data start address
 *
 * @param voiceNum Voice number (0-23)
 * @param startAddr Pointer to start address
 */
void SpuGetVoiceStartAddr(int voiceNum, u_long* startAddr);

/**
 * @brief Set loop start address
 *
 * @param voiceNum Voice number (0-23)
 * @param loopStartAddr Loop start address
 */
void SpuSetVoiceLoopStartAddr(int voiceNum, u_long loopStartAddr);

/**
 * @brief Get loop start address
 *
 * @param voiceNum Voice number (0-23)
 * @param loopStartAddr Pointer to loop start address
 */
void SpuGetVoiceLoopStartAddr(int voiceNum, u_long* loopStartAddr);

/*============================================================================
 * Voice ADSR Functions
 *============================================================================*/

/**
 * @brief Set all ADSR values
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Attack rate
 * @param DR Decay rate
 * @param SR Sustain rate
 * @param RR Release rate
 * @param SL Sustain level
 */
void SpuSetVoiceADSR(
    int voiceNum, u_short AR, u_short DR, u_short SR, u_short RR, u_short SL);

/**
 * @brief Get all ADSR values
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Pointer to attack rate
 * @param DR Pointer to decay rate
 * @param SR Pointer to sustain rate
 * @param RR Pointer to release rate
 * @param SL Pointer to sustain level
 */
void SpuGetVoiceADSR(int voiceNum, u_short* AR, u_short* DR, u_short* SR,
                     u_short* RR, u_short* SL);

/**
 * @brief Set all ADSR values and modes
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Attack rate
 * @param DR Decay rate
 * @param SR Sustain rate
 * @param RR Release rate
 * @param SL Sustain level
 * @param ARmode Attack rate mode
 * @param SRmode Sustain rate mode
 * @param RRmode Release rate mode
 */
void SpuSetVoiceADSRAttr(
    int voiceNum, u_short AR, u_short DR, u_short SR, u_short RR, u_short SL,
    long ARmode, long SRmode, long RRmode);

/**
 * @brief Get all ADSR values and modes
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Pointer to attack rate
 * @param DR Pointer to decay rate
 * @param SR Pointer to sustain rate
 * @param RR Pointer to release rate
 * @param SL Pointer to sustain level
 * @param ARmode Pointer to attack rate mode
 * @param SRmode Pointer to sustain rate mode
 * @param RRmode Pointer to release rate mode
 */
void SpuGetVoiceADSRAttr(
    int voiceNum, u_short* AR, u_short* DR, u_short* SR, u_short* RR,
    u_short* SL, long* ARmode, long* SRmode, long* RRmode);

/**
 * @brief Set attack rate
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Attack rate
 */
void SpuSetVoiceAR(int voiceNum, u_short AR);

/**
 * @brief Get attack rate
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Pointer to attack rate
 */
void SpuGetVoiceAR(int voiceNum, u_short* AR);

/**
 * @brief Set attack rate and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Attack rate
 * @param ARmode Attack rate mode
 */
void SpuSetVoiceARAttr(int voiceNum, u_short AR, long ARmode);

/**
 * @brief Get attack rate and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param AR Pointer to attack rate
 * @param ARmode Pointer to attack rate mode
 */
void SpuGetVoiceARAttr(int voiceNum, u_short* AR, long* ARmode);

/**
 * @brief Set decay rate
 *
 * @param voiceNum Voice number (0-23)
 * @param DR Decay rate
 */
void SpuSetVoiceDR(int voiceNum, u_short DR);

/**
 * @brief Get decay rate
 *
 * @param voiceNum Voice number (0-23)
 * @param DR Pointer to decay rate
 */
void SpuGetVoiceDR(int voiceNum, u_short* DR);

/**
 * @brief Set sustain rate
 *
 * @param voiceNum Voice number (0-23)
 * @param SR Sustain rate
 */
void SpuSetVoiceSR(int voiceNum, u_short SR);

/**
 * @brief Get sustain rate
 *
 * @param voiceNum Voice number (0-23)
 * @param SR Pointer to sustain rate
 */
void SpuGetVoiceSR(int voiceNum, u_short* SR);

/**
 * @brief Set sustain rate and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param SR Sustain rate
 * @param SRmode Sustain rate mode
 */
void SpuSetVoiceSRAttr(int voiceNum, u_short SR, long SRmode);

/**
 * @brief Get sustain rate and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param SR Pointer to sustain rate
 * @param SRmode Pointer to sustain rate mode
 */
void SpuGetVoiceSRAttr(int voiceNum, u_short* SR, long* SRmode);

/**
 * @brief Set sustain level
 *
 * @param voiceNum Voice number (0-23)
 * @param SL Sustain level
 */
void SpuSetVoiceSL(int voiceNum, u_short SL);

/**
 * @brief Get sustain level
 *
 * @param voiceNum Voice number (0-23)
 * @param SL Pointer to sustain level
 */
void SpuGetVoiceSL(int voiceNum, u_short* SL);

/**
 * @brief Set release rate
 *
 * @param voiceNum Voice number (0-23)
 * @param RR Release rate
 */
void SpuSetVoiceRR(int voiceNum, u_short RR);

/**
 * @brief Get release rate
 *
 * @param voiceNum Voice number (0-23)
 * @param RR Pointer to release rate
 */
void SpuGetVoiceRR(int voiceNum, u_short* RR);

/**
 * @brief Set release rate and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param RR Release rate
 * @param RRmode Release rate mode
 */
void SpuSetVoiceRRAttr(int voiceNum, u_short RR, long RRmode);

/**
 * @brief Get release rate and mode
 *
 * @param voiceNum Voice number (0-23)
 * @param RR Pointer to release rate
 * @param RRmode Pointer to release rate mode
 */
void SpuGetVoiceRRAttr(int voiceNum, u_short* RR, long* RRmode);

/**
 * @brief Get current envelope value
 *
 * @param voiceNum Voice number (0-23)
 * @param envx Pointer to envelope value
 */
void SpuGetVoiceEnvelope(int voiceNum, short* envx);

/**
 * @brief Get envelope value and key status
 *
 * @param voiceNum Voice number (0-23)
 * @param keyStat Pointer to key status
 * @param envx Pointer to envelope value
 */
void SpuGetVoiceEnvelopeAttr(int voiceNum, long* keyStat, short* envx);

/*============================================================================
 * Key Control Functions
 *============================================================================*/

/**
 * @brief Set key on/off for voices
 *
 * @param on_off SPU_ON for key on, SPU_OFF for key off
 * @param voice_bit Voice bit mask
 */
void SpuSetKey(long on_off, u_long voice_bit);

/**
 * @brief Set key on with attributes
 *
 * @param attr Voice attributes
 */
void SpuSetKeyOnWithAttr(SpuVoiceAttr* attr);

/**
 * @brief Get key status for a voice
 *
 * @param voice_bit Voice to check
 * @return Key status value
 */
long SpuGetKeyStatus(u_long voice_bit);

/**
 * @brief Get key status for all voices
 *
 * @param status Array of 24 bytes for status
 */
void SpuGetAllKeysStatus(char* status);

/**
 * @brief Get key status for a range of voices
 *
 * @param min Lower limit of voice number
 * @param max Upper limit of voice number
 * @param status Array for status
 * @return SPU_SUCCESS or SPU_INVALID_ARGS
 */
long SpuRGetAllKeysStatus(long min, long max, char* status);

/*============================================================================
 * Common Attribute Functions
 *============================================================================*/

/**
 * @brief Set common attributes
 *
 * @param attr Common attributes
 */
void SpuSetCommonAttr(SpuCommonAttr* attr);

/**
 * @brief Get common attributes
 *
 * @param attr Pointer to common attributes
 */
void SpuGetCommonAttr(SpuCommonAttr* attr);

/**
 * @brief Set master volume
 *
 * @param mvolL Left master volume
 * @param mvolR Right master volume
 */
void SpuSetCommonMasterVolume(short mvolL, short mvolR);

/**
 * @brief Get master volume
 *
 * @param mvolL Pointer to left volume
 * @param mvolR Pointer to right volume
 */
void SpuGetCommonMasterVolume(short* mvolL, short* mvolR);

/**
 * @brief Set master volume and mode
 *
 * @param mvolL Left master volume
 * @param mvolR Right master volume
 * @param mvolModeL Left volume mode
 * @param mvolModeR Right volume mode
 */
void SpuSetCommonMasterVolumeAttr(
    short mvolL, short mvolR, short mvolModeL, short mvolModeR);

/**
 * @brief Get master volume and mode
 *
 * @param mvolL Pointer to left volume
 * @param mvolR Pointer to right volume
 * @param mvolModeL Pointer to left mode
 * @param mvolModeR Pointer to right mode
 */
void SpuGetCommonMasterVolumeAttr(
    short* mvolL, short* mvolR, short* mvolModeL, short* mvolModeR);

/**
 * @brief Get current master volume
 *
 * @param mvolXL Pointer to current left volume
 * @param mvolXR Pointer to current right volume
 */
void SpuGetCommonMasterVolumeX(short* mvolXL, short* mvolXR);

/**
 * @brief Set CD input volume
 *
 * @param cdvolL Left CD volume
 * @param cdvolR Right CD volume
 */
void SpuSetCommonCDVolume(short cdvolL, short cdvolR);

/**
 * @brief Get CD input volume
 *
 * @param cdvolL Pointer to left CD volume
 * @param cdvolR Pointer to right CD volume
 */
void SpuGetCommonCDVolume(short* cdvolL, short* cdvolR);

/**
 * @brief Set CD input mixing on/off
 *
 * @param on_off SPU_ON or SPU_OFF
 */
void SpuSetCommonCDMix(long on_off);

/**
 * @brief Get CD input mixing status
 *
 * @param on_off Pointer to status
 */
void SpuGetCommonCDMix(long* on_off);

/**
 * @brief Set CD input reverb on/off
 *
 * @param on_off SPU_ON or SPU_OFF
 */
void SpuSetCommonCDReverb(long on_off);

/**
 * @brief Get CD input reverb status
 *
 * @param on_off Pointer to status
 */
void SpuGetCommonCDReverb(long* on_off);

/*============================================================================
 * Reverb Functions
 *============================================================================*/

/**
 * @brief Turn reverb on/off
 *
 * @param on_off SPU_ON or SPU_OFF
 * @return Reverb status
 */
long SpuSetReverb(long on_off);

/**
 * @brief Get reverb status
 *
 * @return SPU_ON or SPU_OFF
 */
long SpuGetReverb(void);

/**
 * @brief Set reverb mode and parameters
 *
 * @param attr Reverb attributes
 * @return 0 on success, SPU_ERROR on failure
 */
long SpuSetReverbModeParam(SpuReverbAttr* attr);

/**
 * @brief Get reverb mode and parameters
 *
 * @param attr Pointer to reverb attributes
 */
void SpuGetReverbModeParam(SpuReverbAttr* attr);

/**
 * @brief Set reverb mode type
 *
 * @param type Reverb mode type
 */
void SpuSetReverbModeType(long type);

/**
 * @brief Get reverb mode type
 *
 * @param type Pointer to reverb mode type
 */
void SpuGetReverbModeType(long* type);

/**
 * @brief Set reverb depth
 *
 * @param attr Reverb attributes
 * @return 1
 */
long SpuSetReverbDepth(SpuReverbAttr* attr);

/**
 * @brief Set reverb mode depth
 *
 * @param depthL Left depth
 * @param depthR Right depth
 */
void SpuSetReverbModeDepth(short depthL, short depthR);

/**
 * @brief Get reverb mode depth
 *
 * @param depthL Pointer to left depth
 * @param depthR Pointer to right depth
 */
void SpuGetReverbModeDepth(short* depthL, short* depthR);

/**
 * @brief Set reverb delay time
 *
 * @param delay Delay value (0-127)
 */
void SpuSetReverbModeDelayTime(long delay);

/**
 * @brief Get reverb delay time
 *
 * @param delay Pointer to delay value
 */
void SpuGetReverbModeDelayTime(long* delay);

/**
 * @brief Set reverb feedback
 *
 * @param feedback Feedback value (0-127)
 */
void SpuSetReverbModeFeedback(long feedback);

/**
 * @brief Get reverb feedback
 *
 * @param feedback Pointer to feedback value
 */
void SpuGetReverbModeFeedback(long* feedback);

/**
 * @brief Set reverb on/off for voices
 *
 * @param on_off SPU_ON, SPU_OFF, or SPU_BIT
 * @param voice_bit Voice bit mask
 * @return Current reverb voice status
 */
u_long SpuSetReverbVoice(long on_off, u_long voice_bit);

/**
 * @brief Get reverb voice status
 *
 * @return Voice reverb bit mask
 */
u_long SpuGetReverbVoice(void);

/**
 * @brief Reserve/release reverb work area
 *
 * @param on_off SPU_ON to reserve, SPU_OFF to release
 * @return Status
 */
long SpuReserveReverbWorkArea(long on_off);

/**
 * @brief Check if reverb work area is reserved
 *
 * @param on_off SPU_CHECK or SPU_DIAG
 * @return SPU_ON or SPU_OFF
 */
long SpuIsReverbWorkAreaReserved(long on_off);

/**
 * @brief Clear reverb work area
 *
 * @param rev_mode Reverb mode
 * @return 0 on success, SPU_ERROR on failure
 */
long SpuClearReverbWorkArea(long rev_mode);

/*============================================================================
 * Noise Functions
 *============================================================================*/

/**
 * @brief Set noise source clock
 *
 * @param n_clock Noise clock (0-0x3f)
 * @return Noise clock value
 */
long SpuSetNoiseClock(long n_clock);

/**
 * @brief Get noise source clock
 *
 * @return Noise clock value
 */
long SpuGetNoiseClock(void);

/**
 * @brief Set noise on/off for voices
 *
 * @param on_off SPU_ON, SPU_OFF, or SPU_BIT
 * @param voice_bit Voice bit mask
 * @return Current noise voice status
 */
u_long SpuSetNoiseVoice(long on_off, u_long voice_bit);

/**
 * @brief Get noise voice status
 *
 * @return Voice noise bit mask
 */
u_long SpuGetNoiseVoice(void);

/*============================================================================
 * Pitch LFO Functions
 *============================================================================*/

/**
 * @brief Set pitch LFO on/off for voices
 *
 * @param on_off SPU_ON, SPU_OFF, or SPU_BIT
 * @param voice_bit Voice bit mask
 * @return Current pitch LFO voice status
 */
u_long SpuSetPitchLFOVoice(long on_off, u_long voice_bit);

/**
 * @brief Get pitch LFO voice status
 *
 * @return Voice pitch LFO bit mask
 */
u_long SpuGetPitchLFOVoice(void);

/*============================================================================
 * Mute Functions
 *============================================================================*/

/**
 * @brief Set mute on/off
 *
 * @param on_off SPU_ON or SPU_OFF
 * @return Mute status
 */
long SpuSetMute(long on_off);

/**
 * @brief Get mute status
 *
 * @return SPU_ON or SPU_OFF
 */
long SpuGetMute(void);

/*============================================================================
 * IRQ Functions
 *============================================================================*/

/**
 * @brief Set interrupt request on/off
 *
 * @param on_off SPU_ON, SPU_OFF, or SPU_RESET
 * @return Status set
 */
long SpuSetIRQ(long on_off);

/**
 * @brief Get interrupt request status
 *
 * @return SPU_ON or SPU_OFF
 */
long SpuGetIRQ(void);

/**
 * @brief Set interrupt request address
 *
 * @param addr IRQ address
 * @return Address set
 */
u_long SpuSetIRQAddr(u_long addr);

/**
 * @brief Get interrupt request address
 *
 * @return Current IRQ address
 */
u_long SpuGetIRQAddr(void);

/**
 * @brief Set IRQ callback function
 *
 * @param func Callback function
 * @return Previous callback function
 */
SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc func);

/*============================================================================
 * Transfer Functions
 *============================================================================*/

/**
 * @brief Set transfer mode
 *
 * @param mode Transfer mode
 * @return Mode set
 */
int SpuSetTransferMode(int mode);

/**
 * @brief Get transfer mode
 *
 * @return Current transfer mode
 */
int SpuGetTransferMode(void);

/**
 * @brief Set transfer start address
 *
 * @param addr Start SPU address
 * @return SPU address set
 */
unsigned SpuSetTransferStartAddr(unsigned addr);

/**
 * @brief Get transfer start address
 *
 * @return Current start address
 */
u_long SpuGetTransferStartAddr(void);

/**
 * @brief Transfer data from main memory to sound buffer
 *
 * @param addr Source address in main memory
 * @param size Transfer size in bytes
 * @return Transferred size
 */
u_long SpuWrite(u_char* addr, u_long size);

/**
 * @brief Clear sound buffer
 *
 * @param size Size to clear in bytes
 * @return Cleared size
 */
u_long SpuWrite0(u_long size);

/**
 * @brief Transfer data in sections
 *
 * @param addr Source address in main memory
 * @param size Transfer size in bytes
 * @return Transferred size
 */
u_long SpuWritePartly(u_char* addr, u_long size);

/**
 * @brief Transfer data from sound buffer to main memory
 *
 * @param addr Destination address in main memory
 * @param size Transfer size in bytes
 * @return Transferred size
 */
u_long SpuRead(u_char* addr, u_long size);

/**
 * @brief Read decoded SPU data
 *
 * @param d_data Pointer to decode data structure
 * @param flag SPU_CDONLY, SPU_VOICEONLY, or SPU_ALL
 * @return Buffer area being written to
 */
long SpuReadDecodedData(SpuDecodeData* d_data, long flag);

/**
 * @brief Check if transfer is completed
 *
 * @param flag SPU_TRANSFER_WAIT or SPU_TRANSFER_PEEK
 * @return 1 if completed, 0 if not
 */
long SpuIsTransferCompleted(long flag);

/**
 * @brief Set transfer callback function
 *
 * @param func Callback function
 * @return Previous callback function
 */
SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func);

/*============================================================================
 * Memory Management Functions
 *============================================================================*/

/**
 * @brief Initialize memory management
 *
 * @param num Maximum number of allocations
 * @param top Pointer to management table
 * @return Number of blocks specified
 */
long SpuInitMalloc(long num, char* top);

/**
 * @brief Allocate area in sound buffer
 *
 * @param size Size in bytes
 * @return Start address, or -1 on failure
 */
int SpuMalloc(int size);

/**
 * @brief Allocate area from specific address
 *
 * @param addr Start address
 * @param size Size in bytes
 * @return Start address, or -1 on failure
 */
int SpuMallocWithStartAddr(unsigned addr, int size);

/**
 * @brief Free allocated area
 *
 * @param addr Start address of area
 */
void SpuFree(int addr);

/*============================================================================
 * Environment Functions
 *============================================================================*/

/**
 * @brief Set SPU environment
 *
 * @param env Environment attributes
 */
void SpuSetEnv(SpuEnv* env);

/**
 * @brief Flush queued events
 *
 * @param ev Events to flush
 * @return Flushed events
 */
u_long SpuFlush(u_long ev);

/**
 * @brief Set ESA for straight PCM playback
 *
 * @param revAddr Starting address for PCM playback
 */
void SpuSetESA(long revAddr);

/*============================================================================
 * SPU Streaming Functions
 *============================================================================*/

/**
 * @brief Initialize SPU streaming
 *
 * @param mode Not used, pass 0
 * @return Pointer to streaming environment
 */
SpuStEnv* SpuStInit(long mode);

/**
 * @brief Complete SPU streaming
 *
 * @return SPU_ST_ACCEPT or SPU_ST_WRONG_STATUS
 */
long SpuStQuit(void);

/**
 * @brief Get SPU streaming status
 *
 * @return Streaming status
 */
long SpuStGetStatus(void);

/**
 * @brief Get voices used for streaming
 *
 * @return Voice bit mask
 */
u_long SpuStGetVoiceStatus(void);

/**
 * @brief Prepare/start stream
 *
 * @param flag SPU_ST_PREPARE or SPU_ST_PLAY
 * @param voice_bit Streaming voices
 * @return Status
 */
long SpuStTransfer(long flag, u_long voice_bit);

/**
 * @brief Set preparation finished callback
 *
 * @param callback_proc Callback function
 * @return Previous callback function
 */
SpuStCallbackProc SpuStSetPreparationFinishedCallback(
    SpuStCallbackProc callback_proc);

/**
 * @brief Set transfer finished callback
 *
 * @param callback_proc Callback function
 * @return Previous callback function
 */
SpuStCallbackProc SpuStSetTransferFinishedCallback(
    SpuStCallbackProc callback_proc);

/**
 * @brief Set stream finished callback
 *
 * @param callback_proc Callback function
 * @return Previous callback function
 */
SpuStCallbackProc SpuStSetStreamFinishedCallback(
    SpuStCallbackProc callback_proc);

#endif /* LIBSPU_H */
