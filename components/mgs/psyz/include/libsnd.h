#ifndef LIBSND_H
#define LIBSND_H
#include <psyz/types.h>

/**
 * @file libsnd.h
 * @brief Extended Sound Library (libsnd)
 *
 * This library provides high-level sound functions for the PlayStation,
 * including MIDI sequence playback (SEQ/SEP), VAB bank management,
 * voice control, reverb effects, and sound effect utilities.
 */

/* SEQ/SEP data attribute table size constant */
#if defined(__psyz) && defined(PLATFORM_64BIT)
#define SS_SEQ_TABSIZ 192
#else
#define SS_SEQ_TABSIZ 176
#endif

/* Tick mode constants */
#define SS_NOTICK 0x1000  /**< No automatic tick callback */
#define SS_NOTICK0 0      /**< Legacy: no tick */
#define SS_TICK60 1       /**< 1/60 second tick rate */
#define SS_TICK240 2      /**< 1/240 second tick rate */
#define SS_TICK120 3      /**< 1/120 second tick rate */
#define SS_TICK50 4       /**< 1/50 second tick rate */
#define SS_TICKVSYNC 5    /**< VSync resolution tick rate */
#define SS_TICKMODE_MAX 6 /**< Maximum tick mode value */

/* Transfer completion flags */
#define SS_IMEDIATE 0       /**< Legacy: return immediately */
#define SS_IMMEDIATE 0      /**< Return immediately */
#define SS_WAIT_COMPLETED 1 /**< Wait until transfer completed */

/* Reverb type constants */
#define SS_REV_TYPE_OFF 0      /**< Reverb off */
#define SS_REV_TYPE_ROOM 1     /**< Room reverb */
#define SS_REV_TYPE_STUDIO_A 2 /**< Studio (small) reverb */
#define SS_REV_TYPE_STUDIO_B 3 /**< Studio (medium) reverb */
#define SS_REV_TYPE_STUDIO_C 4 /**< Studio (large) reverb */
#define SS_REV_TYPE_HALL 5     /**< Hall reverb */
#define SS_REV_TYPE_SPACE 6    /**< Space echo */
#define SS_REV_TYPE_ECHO 7     /**< Echo */
#define SS_REV_TYPE_DELAY 8    /**< Delay */
#define SS_REV_TYPE_PIPE 9     /**< Pipe echo */

/* Serial and attribute constants */
#define SS_SOFF 0     /**< Attribute off */
#define SS_SON 1      /**< Attribute on */
#define SS_MIX 0      /**< Mixing attribute */
#define SS_REV 1      /**< Reverb attribute */
#define SS_SERIAL_A 0 /**< Serial A (CD input) */
#define SS_SERIAL_B 1 /**< Serial B (external digital input) */

/* Play mode constants */
#define SSPLAY_PLAY 1     /**< Immediate play */
#define SSPLAY_PAUSE 0    /**< Start paused */
#define SSPLAY_INFINITY 0 /**< Infinite loop */

/* Mute mode constants */
#define SS_MUTE_ON 1  /**< Mute on */
#define SS_MUTE_OFF 0 /**< Mute off */

/* Skip unit constants for SsSeqSkip */
/* Skip unit constants for SsSeqSkip */
#define SSSKIP_TICK 0  /**< Skip in tick units */
#define SSSKIP_NOTE4 1 /**< Skip in quarter note units */
#define SSSKIP_NOTE8 2 /**< Skip in eighth note units */
#define SSSKIP_BAR 3   /**< Skip in measure units */

/* SndRegisterAttr mask values */
#define SND_VOLL 0x0001  /**< Left volume */
#define SND_VOLR 0x0002  /**< Right volume */
#define SND_PITCH 0x0004 /**< Pitch */
#define SND_ADDR 0x0008  /**< Waveform data start address */
#define SND_ADSR1 0x0010 /**< ADSR1 information */
#define SND_ADSR2 0x0020 /**< ADSR2 information */

/* Control change index constants for _SsFCALL */
#define CC_NUMBER 0     /**< Control change */
#define CC_BANKCHANGE 1 /**< Bank change (CC#1) */
#define CC_DATAENTRY 2  /**< Data entry (CC#6) */
#define CC_MAINVOL 3    /**< Main volume (CC#7) */
#define CC_PANPOT 4     /**< Pan pot (CC#10) */
#define CC_EXPRESSION 5 /**< Expression (CC#11) */
#define CC_DAMPER 6     /**< Damper pedal (CC#64) */
#define CC_NRPN1 7      /**< NRPN (CC#98) */
#define CC_NRPN2 8      /**< NRPN (CC#99) */
#define CC_RPN1 9       /**< RPN (CC#100) */
#define CC_RPN2 10      /**< RPN (CC#101) */
#define CC_EXTERNAL 11  /**< External effect depth (CC#91) */
#define CC_RESETALL 12  /**< Reset all (CC#121) */

/* Data entry index constants */
#define DE_PRIORITY 0     /**< Priority */
#define DE_MODE 1         /**< Mode */
#define DE_LIMITL 2       /**< Limit low */
#define DE_LIMITH 3       /**< Limit high */
#define DE_ADSR_AR_L 4    /**< ADSR AR-L */
#define DE_ADSR_AR_E 5    /**< ADSR AR-E */
#define DE_ADSR_DR 6      /**< ADSR DR */
#define DE_ADSR_SL 7      /**< ADSR SL */
#define DE_ADSR_SR_L 8    /**< ADSR SR-L */
#define DE_ADSR_SR_E 9    /**< ADSR SR-E */
#define DE_ADSR_RR_L 10   /**< ADSR RR-L */
#define DE_ADSR_RR_E 11   /**< ADSR RR-E */
#define DE_ADSR_SR 12     /**< ADSR SR */
#define DE_VIB_TIME 13    /**< Vibrato time (not supported) */
#define DE_PORTA_DEPTH 14 /**< Portamento depth (not supported) */
#define DE_REV_TYPE 15    /**< Reverb type */
#define DE_REV_DEPTH 16   /**< Reverb depth */
#define DE_ECHO_FB 17     /**< Echo feedback */
#define DE_ECHO_DELAY 18  /**< Echo delay */
#define DE_DELAY 19       /**< Delay time */

typedef void (*SndTickCallback)(void);

/**
 * @brief Volume structure
 *
 * Standard volume structure with left/right channel values (0-127).
 */
typedef struct SndVolume {
    u_short left;  /**< L channel volume value (0-127) */
    u_short right; /**< R channel volume value (0-127) */
} SndVolume;

/**
 * @brief Extended volume structure
 *
 * Volume structure with extended range for advanced keyon emulation.
 * Supports negative volumes for phase inversion effects.
 */
typedef struct SndVolume2 {
    short left;  /**< Left volume value (-0x4000 to 0x3fff) */
    short right; /**< Right volume value (-0x4000 to 0x3fff) */
} SndVolume2;

/**
 * @brief SPU register attributes
 *
 * Used in SsQueueRegisters() to set SPU voice information.
 */
typedef struct SndRegisterAttr {
    SndVolume2 volume; /**< Volume data for left and right channels */
    short pitch;       /**< Pitch rate at which to play back waveform data */
    short mask;        /**< Bitfield designating which attributes to set */
    short addr;        /**< Waveform data start address */
    short adsr1;       /**< ADSR1 bitfield */
    short adsr2;       /**< ADSR2 bitfield */
} SndRegisterAttr;

/**
 * @brief Internal libsnd voice variables
 *
 * Used to fill internal libsnd voice structures in SsSetVoiceSettings().
 */
typedef struct SndVoiceStats {
    short vagId;   /**< VAG number pointed to by tone information (1-254) */
    short vabId;   /**< VAB number containing tone information (0-15) */
    u_short pitch; /**< Playback rate of voice */
    short vol;     /**< Volume of voice (0-127). Not valid for 3D sound */
    char pan;      /**< Voice pan (0=left, 64=center, 127=right). No 3D sound */
    short note;    /**< Note at which tone information keyed on */
    short tone;    /**< Tone number (0-15) */
    short prog_num; /**< Program number containing tone information (0-127) */
    short prog_actual; /**< The "real" program number (offset in VAB header) */
} SndVoiceStats;

/**
 * @brief Program header
 *
 * Contains program-level attributes within a VAB bank.
 */
typedef struct ProgAtr {
    u_char tones; /**< Number of VAG attribute sets contained in the program */
    u_char mvol;  /**< Master volume for the program */
    u_char prior; /**< Program priority (0-15) */
    u_char mode;  /**< Sound source mode */
    u_char mpan;  /**< Program pan */
    char reserved0;         /**< Reserved by the system */
    short attr;             /**< Program attribute */
    unsigned int reserved1; /**< Reserved by the system */
    unsigned int reserved2; /**< Reserved by the system */
} ProgAtr;

/**
 * @brief Bank header
 *
 * VAB header containing bank-level information such as sound source
 * data set size and sound source numerals used at runtime.
 */
typedef struct VabHdr {
    int form;           /**< Format name (always 'VABp') */
    int ver;            /**< Format version number */
    int id;             /**< Bank (VAB) number */
    unsigned int fsize; /**< Bank file size */
    u_short reserved0;  /**< Reserved by the system */
    u_short ps;         /**< Total number of programs contained in the bank */
    u_short ts;         /**< Total number of tones contained in the bank */
    u_short vs;         /**< Number of VAGs contained in the bank */
    u_char mvol;        /**< Master volume */
    u_char pan;         /**< Master pan level */
    u_char attr1;       /**< Bank attribute 1 (user definable) */
    u_char attr2;       /**< Bank attribute 2 (user definable) */
    unsigned int reserved1; /**< Reserved by the system */
} VabHdr;

/**
 * @brief Waveform header (tone attributes)
 *
 * Contains per-tone attributes within a program.
 */
typedef struct VagAtr {
    u_char prior;      /**< Priority (0-15) */
    u_char mode;       /**< Sound source mode (0=normal, 4=reverb) */
    u_char vol;        /**< Volume (0-127, 0=min, 127=max) */
    u_char pan;        /**< Pan pot (0=left, 63=center, 127=right) */
    u_char center;     /**< Center note (0-127) */
    u_char shift;      /**< Pitch correction in cents (0-127) */
    u_char min;        /**< Minimum note limit */
    u_char max;        /**< Maximum note limit */
    u_char vibW;       /**< Vibrato width (0-127) (not implemented) */
    u_char vibT;       /**< Vibrato cycle period in ticks (not implemented) */
    u_char porW;       /**< Portamento width (not implemented) */
    u_char porT;       /**< Portamento duration in ticks (not implemented) */
    u_char pbmin;      /**< Minimum pitch bend limit */
    u_char pbmax;      /**< Maximum pitch bend limit */
    u_char reserved1;  /**< Reserved by the system */
    u_char reserved2;  /**< Reserved by the system */
    u_short adsr1;     /**< ADSR value 1 */
    u_short adsr2;     /**< ADSR value 2 */
    short prog;        /**< Master program containing the VAG attribute */
    short vag;         /**< VAG's ID number utilized by the VAG attribute */
    short reserved[4]; /**< Reserved by the system */
} VagAtr;

typedef void (*sCb)();

/**
 * @brief Function table for low-level MIDI functions
 *
 * Used with SsSeqOpenJ() and SsSepOpenJ() to selectively link
 * only needed low-level MIDI functions, reducing code size.
 */
typedef struct _SsFCALL {
    sCb noteon;        /**< Note on function */
    sCb programchange; /**< Program change function */
    sCb pitchbend;     /**< Pitch bend function */
    sCb metaevent;     /**< Meta event function */
    sCb control[13];   /**< Control change functions */
    sCb ccentry[20];   /**< CC entry functions */
} _SsFCALL;

/** Global function table variable */
extern _SsFCALL SsFCALL;

/* Callback function type */
typedef void (*SsMarkCallbackProc)(short, short, short);

/*============================================================================
 * Initialization and System Functions
 *============================================================================*/

/**
 * @brief Initialize sound system
 *
 * Initializes the sound system, clearing the sound local memory.
 */
void SsInit(void);

/**
 * @brief Initialize sound system (hot reset)
 *
 * Initializes the sound system without destroying data that has been
 * transferred to the sound buffer. Use when a child process wants to
 * initialize the sound system with the sound buffer in its current state.
 */
void SsInitHot(void);

/**
 * @brief Start the sound system
 *
 * Starts the sound system. When SsSetTickMode() is used to set a mode
 * that calls SsSeqCalledTbyT() automatically, this function causes
 * SsSeqCalledTbyT() to be called each tick.
 */
void SsStart(void);

/**
 * @brief Start the sound system (VSyncCallback version)
 *
 * Starts the sound system. Must be used when tick mode equals TV sync rate.
 */
void SsStart2(void);

/**
 * @brief Stop the sound system
 *
 * Stops SsSeqCalledTbyT() from being called at every tick when using
 * automatic tick mode.
 */
void SsEnd(void);

/**
 * @brief Terminate the sound system
 *
 * Terminates the sound system; transfer to the sound buffer is disabled.
 * SsEnd() must be called before SsQuit().
 */
void SsQuit(void);

/**
 * @brief Set tick mode
 *
 * Sets the resolution of a tick. Call only once before SsSeqOpen(),
 * SsSepOpen(), or SsStart().
 *
 * @param tick_mode Tick mode (SS_TICK50, SS_TICK60, SS_TICKVSYNC,
 *                  SS_TICK120, SS_TICK240, SS_NOTICK, or 60-240)
 */
void SsSetTickMode(long tick_mode);

/**
 * @brief Specify SEQ/SEP data attribute table area
 *
 * @param table Pointer to SEQ/SEP data attribute table area
 * @param s_max Maximum times to open SEQ/SEP data (up to 32)
 * @param t_max Number of SEQ included in SEP (max 16)
 */
void SsSetTableSize(char* table, short s_max, short t_max);

/**
 * @brief Interpret SEQ/SEP data and carry out playback
 *
 * Called at each tick to interpret SEQ/SEP data and carry out playback.
 * When tick mode is SS_NOTICK, the program must call this manually.
 */
void SsSeqCalledTbyT(void);

/**
 * @brief Set the tick callback function
 *
 * Defines a function to be called every tick.
 *
 * @param cb Pointer to tick callback function
 * @return Previously set tick callback function
 */
SndTickCallback SsSetTickCallback(SndTickCallback cb);

/*============================================================================
 * VAB (Voice Attribute Bank) Functions
 *============================================================================*/

/**
 * @brief Open VAB data (not recommended)
 *
 * @deprecated Use SsVabOpenHead() and SsVabTransBody() instead.
 * @param addr Pointer to VAB data in main storage
 * @param vab_header Pointer to VAB header structure
 * @return VAB id, or -1 on failure
 */
short SsVabOpen(u_char* addr, VabHdr* vab_header);

/**
 * @brief Open a VAB header
 *
 * Sets up a VAB header list in main memory. Execute SsVabTransBody()
 * to transfer the VAB body to SPU RAM.
 *
 * @param addr Start address of VAB header (.VH) in main memory
 * @param vabid VAB ID (0-15), or -1 to auto-allocate
 * @return VAB identification number, or -1 on failure
 */
short SsVabOpenHead(u_char* addr, short vabid);

/**
 * @brief Open a VAB header with specified transfer address
 *
 * Sets up a VAB header and specifies the sound buffer transfer address.
 *
 * @param addr Start address of VAB header (.VH) in main memory
 * @param vabid Desired VAB ID or -1 to auto-allocate
 * @param sbaddr Start address in sound buffer for VabBody
 * @return VAB identification number, or -1 on failure
 */
short SsVabOpenHeadSticky(u_char* addr, short vabid, u_long sbaddr);

/**
 * @brief Transfer sound source data
 *
 * Transfers VAB body data to SPU local memory after header recognition.
 *
 * @param addr VAB data start address
 * @param vabid VAB ID
 * @return VAB identification number, or -1 on failure
 */
short SsVabTransBody(u_char* addr, short vabid);

/**
 * @brief Transfer sound source data in segments
 *
 * Transfers VAB body in segments to conserve main memory.
 *
 * @param addr Pointer to segment transfer buffer
 * @param bufsize Buffer size (must be multiple of 64)
 * @param vabid VAB ID
 * @return vabid on success, -2 if incomplete, -1 on failure
 */
short SsVabTransBodyPartly(u_char* addr, u_long bufsize, short vabid);

/**
 * @brief Get VAB data transfer state
 *
 * @param immediateFlag SS_IMMEDIATE to return immediately,
 *                      SS_WAIT_COMPLETED to wait for completion
 * @return 1 if completed, 0 if ongoing
 */
short SsVabTransCompleted(short immediateFlag);

/**
 * @brief Recognize and transfer sound source data
 *
 * @param vh_addr Pointer to VH data
 * @param vb_addr Pointer to VB data
 * @param vabid VAB ID number, or -1 to auto-allocate
 * @param i_flag SS_IMMEDIATE or SS_WAIT_COMPLETED
 * @return VAB ID number, or negative error code
 */
short SsVabTransfer(
    u_char* vh_addr, u_char* vb_addr, short vabid, short i_flag);

/**
 * @brief Open a new VAB header for a given VAB body
 *
 * Associates new VAB header data with an existing VabBody.
 *
 * @param addr Pointer to VH leading address
 * @param vabid Desired VAB ID or -1 to auto-allocate
 * @param sbaddr Address of VB inside the sound buffer
 * @return VAB identification number, or -1 on failure
 */
short SsVabFakeHead(u_char* addr, short vabid, u_long sbaddr);

/**
 * @brief Assign a VAB ID to VAB data in the sound buffer
 *
 * Assigns a VAB ID to VAB body data already in sound buffer.
 *
 * @param vabid VAB id
 * @return VAB identification number, or -1 on failure
 */
short SsVabFakeBody(short vabid);

/**
 * @brief Close VAB data file
 *
 * @param vab_id VAB data id
 */
void SsVabClose(short vab_id);

/*============================================================================
 * SEQ (Sequence) Functions
 *============================================================================*/

/**
 * @brief Open SEQ data
 *
 * @param addr Pointer to SEQ data in main storage
 * @param vab_id VAB id
 * @return SEQ access number, or -1 if more than 32 open
 */
short SsSeqOpen(u_long* addr, short vab_id);

/**
 * @brief Open SEQ data (function table version)
 *
 * Enables selective linking of low-level MIDI functions.
 *
 * @param addr Pointer to SEQ data in main storage
 * @param vab_id VAB id
 * @return SEQ access number
 */
short SsSeqOpenJ(u_long* addr, short vab_id);

/**
 * @brief Close SEQ data
 *
 * @param seq_access_num SEQ access number
 */
void SsSeqClose(short seq_access_num);

/**
 * @brief Read (play) SEQ data
 *
 * @param seq_access_num SEQ access number
 * @param play_mode SSPLAY_PLAY or SSPLAY_PAUSE
 * @param l_count Number of repeats (SSPLAY_INFINITY for unlimited)
 */
void SsSeqPlay(short seq_access_num, char play_mode, short l_count);

/**
 * @brief Read SEQ data (play interval)
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number within SEP (0 for SEQ data)
 * @param start_point Load start point
 * @param end_point Load end point
 * @param play_mode Performance mode
 * @param l_count Loop count
 */
void SsSeqPlayPtoP(short access_num, short seq_num, u_char* start_point,
                   u_char* end_point, char play_mode, short l_count);

/**
 * @brief Pause SEQ data reading
 *
 * @param seq_access_num SEQ access number
 */
void SsSeqPause(short seq_access_num);

/**
 * @brief Resume SEQ data reading
 *
 * @param seq_access_num SEQ access number
 */
void SsSeqReplay(short seq_access_num);

/**
 * @brief Terminate SEQ data reading
 *
 * @param seq_access_num SEQ access number
 */
void SsSeqStop(short seq_access_num);

/**
 * @brief Set SEQ volume
 *
 * @param seq_access_num SEQ access number
 * @param voll Left channel main volume (0-127)
 * @param volr Right channel main volume (0-127)
 */
void SsSeqSetVol(short seq_access_num, short voll, short volr);

/**
 * @brief Get SEQ volume
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number of SEP data (0 for SEQ)
 * @param voll Pointer to left volume
 * @param volr Pointer to right volume
 */
void SsSeqGetVol(short access_num, short seq_num, short* voll, short* volr);

/**
 * @brief Crescendo (increase volume)
 *
 * @param seq_access_num SEQ access number
 * @param vol Volume value (0-127)
 * @param v_time Time in ticks
 */
void SsSeqSetCrescendo(short seq_access_num, short vol, long v_time);

/**
 * @brief Decrescendo (decrease volume)
 *
 * @param seq_access_num SEQ access number
 * @param vol Volume value (0-127)
 * @param v_time Time in ticks
 */
void SsSeqSetDecrescendo(short seq_access_num, short vol, long v_time);

/**
 * @brief Accelerando (increase tempo)
 *
 * @param seq_access_num SEQ access number
 * @param tempo Desired tempo
 * @param v_time Time in ticks
 */
void SsSeqSetAccelerando(short seq_access_num, long tempo, long v_time);

/**
 * @brief Ritardando (decrease tempo)
 *
 * @param seq_access_num SEQ access number
 * @param tempo Desired tempo
 * @param v_time Time in ticks
 */
void SsSeqSetRitardando(short seq_access_num, long tempo, long v_time);

/**
 * @brief Specify next SEQ data to play
 *
 * @param seq_access_num1 Current SEQ access number
 * @param seq_access_num2 Next SEQ access number
 */
void SsSeqSetNext(short seq_access_num1, short seq_access_num2);

/**
 * @brief Skip SEQ data
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number within SEP (0 for SEQ)
 * @param unit Skip unit (SSSKIP_TICK, SSSKIP_NOTE4, SSSKIP_NOTE8, SSSKIP_BAR)
 * @param count Skip amount
 * @return 0 on success, -1 on failure
 */
int SsSeqSkip(short access_num, short seq_num, char unit, short count);

/*============================================================================
 * SEP (Sequence with multiple tracks) Functions
 *============================================================================*/

/**
 * @brief Open SEP data
 *
 * @param addr Pointer to SEP data in main memory
 * @param vab_id VAB id
 * @param seq_num Number of SEQs contained in SEP
 * @return SEP access number
 */
short SsSepOpen(unsigned long* addr, short vab_id, short seq_num);

/**
 * @brief Open SEP data (function table version)
 *
 * @param addr Pointer to SEP data in main memory
 * @param vab_id VAB id
 * @param seq_num Number of SEQs contained in SEP
 * @return SEP access number
 */
short SsSepOpenJ(u_long* addr, short vab_id, short seq_num);

/**
 * @brief Close SEP data
 *
 * @param sep_access_num SEP access number
 */
void SsSepClose(short sep_access_num);

/**
 * @brief Read (play) SEP data
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 * @param play_mode SSPLAY_PLAY or SSPLAY_PAUSE
 * @param l_count Song repetition count
 */
void SsSepPlay(
    short sep_access_num, short seq_num, char play_mode, short l_count);

/**
 * @brief Pause SEP data reading
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 */
void SsSepPause(short sep_access_num, short seq_num);

/**
 * @brief Resume SEP data reading
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 */
void SsSepReplay(short sep_access_num, short seq_num);

/**
 * @brief Stop SEP data reading
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 */
void SsSepStop(short sep_access_num, short seq_num);

/**
 * @brief Set SEP volume
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 * @param voll L channel main volume (0-127)
 * @param volr R channel main volume (0-127)
 */
void SsSepSetVol(short sep_access_num, short seq_num, short voll, short volr);

/**
 * @brief Crescendo for SEP
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 * @param vol Volume value (0-127)
 * @param v_time Time in ticks
 */
void SsSepSetCrescendo(
    short sep_access_num, short seq_num, short vol, long v_time);

/**
 * @brief Decrescendo for SEP
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 * @param vol Volume value (0-127)
 * @param v_time Time in ticks
 */
void SsSepSetDecrescendo(
    short sep_access_num, short seq_num, short vol, long v_time);

/**
 * @brief Accelerando for SEP
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 * @param tempo Desired tempo
 * @param v_time Time in ticks
 */
void SsSepSetAccelerando(
    short sep_access_num, short seq_num, long tempo, long v_time);

/**
 * @brief Ritardando for SEP
 *
 * @param sep_access_num SEP access number
 * @param seq_num SEQ number inside SEP
 * @param tempo Desired tempo
 * @param v_time Time in ticks
 */
void SsSepSetRitardando(
    short sep_access_num, short seq_num, long tempo, long v_time);

/*============================================================================
 * Common SEQ/SEP Functions
 *============================================================================*/

/**
 * @brief Restart song from beginning
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number (0 for SEQ data)
 * @param l_count Song repetition count
 */
void SsPlayBack(short access_num, short seq_num, short l_count);

/**
 * @brief Determine whether song is playing
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number inside SEP (0 for SEQ)
 * @return 1 if playing, 0 if not
 */
short SsIsEos(short access_num, short seq_num);

/**
 * @brief Get current position in SEQ/SEP data
 *
 * @param acn SEP access number
 * @param trn SEQ number within SEP (0 for SEQ)
 * @return SEP/SEQ data address
 */
u_char* SsGetCurrentPoint(short acn, short trn);

/**
 * @brief Set data address in SEQ/SEP
 *
 * @param acn SEP access number
 * @param trn SEQ number within SEP (0 for SEQ)
 * @param point SEQ/SEP data address
 * @return 0 on success, -1 on failure
 */
int SsSetCurrentPoint(short acn, short trn, u_char* point);

/**
 * @brief Select MIDI channels for muting
 *
 * @param acn SEP access number
 * @param trn SEQ number within SEP (0 for SEQ)
 * @param channels MIDI channel bitmask (1=muted)
 */
void SsChannelMute(short acn, short trn, long channels);

/**
 * @brief Get muted channel number
 *
 * @param sep_num SEQ/SEP access number
 * @param seq_num SEQ number within SEP
 * @return Bit field of muted MIDI channels
 */
long SsGetChannelMute(short sep_num, short seq_num);

/**
 * @brief Set song repetition count
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number inside SEP (0 for SEQ)
 * @param l_count Song repetition count
 */
void SsSetLoop(short access_num, short seq_num, short l_count);

/**
 * @brief Set next SEQ/SEP data
 *
 * @param ac_no1 Current SEP/SEQ access number
 * @param tr_no1 SEQ number in SEP (0 for SEQ)
 * @param ac_no2 Next SEP/SEQ access number
 * @param tr_no2 SEQ number in SEP (0 for SEQ)
 */
void SsSetNext(short ac_no1, short tr_no1, short ac_no2, short tr_no2);

/**
 * @brief Set tempo
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number inside SEP (0 for SEQ)
 * @param tempo Song tempo
 */
void SsSetTempo(short access_num, short seq_num, short tempo);

/**
 * @brief Register mark detection callback
 *
 * @param access_num SEQ/SEP access number
 * @param seq_num SEQ number inside SEP (0 for SEQ)
 * @param proc Callback function (NULL to clear)
 */
void SsSetMarkCallback(
    short access_num, short seq_num, SsMarkCallbackProc proc);

/*============================================================================
 * Volume and Audio Mode Functions
 *============================================================================*/

/**
 * @brief Set main volume value
 *
 * Sets master volume for the sound system. Must be called before playing.
 *
 * @param voll L channel volume (0-127)
 * @param volr R channel volume (0-127)
 */
void SsSetMVol(short voll, short volr);

/**
 * @brief Get main volume value
 *
 * @param m_vol Pointer to volume structure
 */
void SsGetMVol(SndVolume* m_vol);

/**
 * @brief Set reverb volume values
 *
 * @param voll L channel volume (0-127)
 * @param volr R channel volume (0-127)
 */
void SsSetRVol(short voll, short volr);

/**
 * @brief Get reverb volume value
 *
 * @param r_vol Pointer to reverb volume structure
 */
void SsGetRVol(SndVolume* r_vol);

/**
 * @brief Set muting
 *
 * @param mode SS_MUTE_ON or SS_MUTE_OFF
 */
void SsSetMute(char mode);

/**
 * @brief Get mute attribute
 *
 * @return SS_MUTE_ON or SS_MUTE_OFF
 */
char SsGetMute(void);

/**
 * @brief Set monaural mode
 *
 * Forces all libsnd keyed-on voices to have equivalent left and right volumes.
 */
void SsSetMono(void);

/**
 * @brief Set stereo mode
 *
 * The sound system default output is stereo.
 */
void SsSetStereo(void);

/**
 * @brief Set automatic KeyOff mode
 *
 * @param mode 0 = auto key off, 1 = wait for explicit key off
 */
void SsSetAutoKeyOffMode(short mode);

/*============================================================================
 * Serial Input Functions (CD Audio / External Digital)
 *============================================================================*/

/**
 * @brief Set serial attribute
 *
 * @param s_num SS_SERIAL_A (CD) or SS_SERIAL_B (external)
 * @param attr SS_MIX or SS_REV
 * @param mode SS_SON or SS_SOFF
 */
void SsSetSerialAttr(char s_num, char attr, char mode);

/**
 * @brief Get serial attribute
 *
 * @param s_num SS_SERIAL_A or SS_SERIAL_B
 * @param attr SS_MIX or SS_REV
 * @return 1 if on, 0 if off
 */
char SsGetSerialAttr(char s_num, char attr);

/**
 * @brief Set serial volume
 *
 * @param s_num SS_SERIAL_A or SS_SERIAL_B
 * @param voll L channel volume (0-127)
 * @param volr R channel volume (0-127)
 */
void SsSetSerialVol(char s_num, short voll, short volr);

/**
 * @brief Get serial volume
 *
 * @param s_num SS_SERIAL_A or SS_SERIAL_B
 * @param s_vol Pointer to volume structure
 */
void SsGetSerialVol(char s_num, SndVolume* s_vol);

/*============================================================================
 * Voice Allocation Functions
 *============================================================================*/

/**
 * @brief Declare number of voices for libsnd allocation
 *
 * Voice numbers are reserved from the lower end. Voices beyond the
 * count are available for libspu or SsUtKeyOnV().
 *
 * @param voices Voice count
 * @return Set voice count, or -1 on failure
 */
char SsSetReservedVoice(char voices);

/**
 * @brief Block voices from allocation system
 *
 * @param s_voice Bitmask of voices to block (SPU_0CH..SPU_23CH)
 */
void SsSetVoiceMask(u_long s_voice);

/**
 * @brief Get blocked voices mask
 *
 * @return Bitmask of blocked voices
 */
u_long SsGetVoiceMask(void);

/**
 * @brief Block voice allocation system
 *
 * Blocks voice allocation for SsUtKeyOn(), SsUtKeyOnV(), and MIDI.
 *
 * @return 1 on success, -1 if already blocked
 */
char SsBlockVoiceAllocation(void);

/**
 * @brief Release voice allocation system
 *
 * @return 1 on success, -1 if not blocked
 */
char SsUnBlockVoiceAllocation(void);

/**
 * @brief Allocate voices by priority
 *
 * @param voices Number of desired voices
 * @param priority Priority (0-127, 127 = highest)
 * @return Bitmask of allocated voices
 */
long SsAllocateVoices(u_char voices, u_char priority);

/**
 * @brief Set internal libsnd variables for a voice
 *
 * @param voice Voice number (0-23)
 * @param Snd_v_attr Voice attributes
 */
void SsSetVoiceSettings(long voice, SndVoiceStats* Snd_v_attr);

/**
 * @brief Verify tone information for a voice
 *
 * @param voice Voice number (0-23)
 * @param vabId VAB ID and program number packed
 * @param note Note at which tone was keyed on
 * @return 0 if valid, -1 if different or out of range
 */
short SsVoiceCheck(long voice, long vabId, short note);

/*============================================================================
 * Voice Queue Functions
 *============================================================================*/

/**
 * @brief Queue SPU register values
 *
 * @param voice Voice number (0-23)
 * @param SRA Register attributes
 */
void SsQueueRegisters(long voice, SndRegisterAttr* SRA);

/**
 * @brief Queue voices for key on
 *
 * @param voices Bitmask of voices to key on
 */
void SsQueueKeyOn(long voices);

/**
 * @brief Queue reverb changes
 *
 * @param voices Bitmask of voices to affect
 * @param reverb Reverb on/off bitmask
 */
void SsQueueReverb(long voices, long reverb);

/*============================================================================
 * Utility Key On/Off Functions
 *============================================================================*/

/**
 * @brief Key on a voice
 *
 * @param vabId VAB number (0-31)
 * @param prog Program number (0-127)
 * @param tone Tone number (0-15)
 * @param note Pitch in semitones (0-127)
 * @param fine Fine pitch (100/127 cents, 0-127)
 * @param voll Left volume (0-127)
 * @param volr Right volume (0-127)
 * @return Voice number (0-23), or -1 on failure
 */
short SsUtKeyOn(short vabId, short prog, short tone, short note, short fine,
                short voll, short volr);

/**
 * @brief Key on a specific voice
 *
 * @param voice Voice number (0-23)
 * @param vabId VAB number (0-31)
 * @param prog Program number (0-127)
 * @param tone Tone number (0-15)
 * @param note Pitch in semitones (0-127)
 * @param fine Fine pitch (100/127 cents, 0-127)
 * @param voll Left volume (0-127)
 * @param volr Right volume (0-127)
 * @return Voice number, or -1 on failure
 */
short SsUtKeyOnV(short voice, short vabId, short prog, short tone, short note,
                 short fine, short voll, short volr);

/**
 * @brief Key off a voice
 *
 * @param voice Voice number (0-23)
 * @param vabId VAB number (0-31)
 * @param prog Program number (0-127)
 * @param tone Tone number (0-15)
 * @param note Pitch in semitones (0-127)
 * @return 0 on success, -1 on failure
 */
short SsUtKeyOff(short voice, short vabId, short prog, short tone, short note);

/**
 * @brief Key off a voice by number
 *
 * @param voice Voice number (0-23)
 * @return 0 on success, -1 on failure
 */
short SsUtKeyOffV(short voice);

/**
 * @brief Key off all voices
 *
 * @param mode Always 0
 */
void SsUtAllKeyOff(short mode);

/**
 * @brief Key on (packed parameters)
 *
 * @param vab_pro VAB id (upper 8 bits) and program (lower 8 bits)
 * @param pitch Key number (upper 8 bits) and fine (lower 8 bits)
 * @param volL L channel volume
 * @param volR R channel volume
 * @return Bitmask of keyed on voices
 */
long SsVoKeyOn(long vab_pro, long pitch, u_short volL, u_short volR);

/**
 * @brief Key off (packed parameters)
 *
 * @param vab_pro VAB id and program number
 * @param pitch Pitch
 * @return Keyed off voice number
 */
long SsVoKeyOff(long vab_pro, long pitch);

/**
 * @brief Execute remaining queued KeyOn/KeyOff requests
 */
void SsUtFlush(void);

/*============================================================================
 * Voice Modification Functions
 *============================================================================*/

/**
 * @brief Set voice volume
 *
 * @param vc Voice number (0-23)
 * @param voll Left volume (0-127)
 * @param volr Right volume (0-127)
 * @return 0 on success, -1 on failure
 */
short SsUtSetVVol(short vc, short voll, short volr);

/**
 * @brief Get voice volume
 *
 * @param vc Voice number (0-23)
 * @param voll Pointer to left volume
 * @param volr Pointer to right volume
 * @return 0 on success, -1 on failure
 */
short SsUtGetVVol(short vc, short* voll, short* volr);

/**
 * @brief Set detailed voice volume
 *
 * @param vc Voice number (0-23)
 * @param detvoll Detailed left volume (0-16383)
 * @param detvolr Detailed right volume (0-16383)
 * @return 0 on success, -1 on failure
 */
short SsUtSetDetVVol(short vc, short detvoll, short detvolr);

/**
 * @brief Get detailed voice volume
 *
 * @param vc Voice number (0-23)
 * @param detvoll Pointer to detailed left volume
 * @param detvolr Pointer to detailed right volume
 * @return 0 on success, -1 on failure
 */
short SsUtGetDetVVol(short vc, short* detvoll, short* detvolr);

/**
 * @brief Automatically change voice volume
 *
 * @param vc Voice number (0-23)
 * @param start_vol Start volume (0-127)
 * @param end_vol End volume (0-127)
 * @param delta_time Change time in ticks (0-10800)
 * @return 0 on success, -1 on failure
 */
short SsUtAutoVol(short vc, short start_vol, short end_vol, short delta_time);

/**
 * @brief Automatically change panning
 *
 * @param vc Voice number (0-23)
 * @param start_pan Start pan (0-127)
 * @param end_pan End pan (0-127)
 * @param delta_time Change time in ticks (0-10800)
 * @return 0 on success, -1 on failure
 */
short SsUtAutoPan(short vc, short start_pan, short end_pan, short delta_time);

/**
 * @brief Change pitch
 *
 * @param voice Voice number (0-23)
 * @param vabId VAB number (0-31)
 * @param prog Program number (0-127)
 * @param old_note Previous pitch (0-127)
 * @param old_fine Previous fine pitch (0-127)
 * @param new_note New pitch (0-127)
 * @param new_fine New fine pitch (0-127)
 * @return 0 on success, -1 on failure
 */
short SsUtChangePitch(short voice, short vabId, short prog, short old_note,
                      short old_fine, short new_note, short new_fine);

/**
 * @brief Apply pitch bend
 *
 * @param voice Voice number (0-23)
 * @param vabId VAB number (0-31)
 * @param prog Program number (0-127)
 * @param note Pitch in semitones (0-127)
 * @param pbend Pitch bend value (0-127, 64=center)
 * @return 0 on success, -1 on failure
 */
short SsUtPitchBend(
    short voice, short vabId, short prog, short note, short pbend);

/**
 * @brief Change ADSR
 *
 * @param vc Voice number (0-23)
 * @param vabId VAB number (0-31)
 * @param prog Program number (0-127)
 * @param old_note Previous pitch (0-127)
 * @param adsr1 ADSR1 value
 * @param adsr2 ADSR2 value
 * @return 0 on success, -1 on failure
 */
short SsUtChangeADSR(short vc, short vabId, short prog, short old_note,
                     u_short adsr1, u_short adsr2);

/**
 * @brief Convert MIDI note to pitch rate
 *
 * @param note MIDI note number (0-127)
 * @param fine Fine tuning (0-127)
 * @param center Center note (0-127)
 * @param shift Pitch correction (0-127)
 * @return Calculated pitch value
 */
u_short SsPitchFromNote(short note, short fine, u_char center, u_char shift);

/**
 * @brief Convert program number to actual program offset
 *
 * @param vabId VAB number
 * @param ProgNum Program number
 * @return Actual program number, or -1 on failure
 */
short SsGetActualProgFromProg(short vabId, short ProgNum);

/*============================================================================
 * Reverb Functions
 *============================================================================*/

/**
 * @brief Set reverb type
 *
 * @param type Reverb type (SS_REV_TYPE_*)
 * @return Set type number, or -1 on failure
 */
short SsUtSetReverbType(short type);

/**
 * @brief Get reverb type
 *
 * @return Current reverb type value
 */
short SsUtGetReverbType(void);

/**
 * @brief Turn on reverb
 */
void SsUtReverbOn(void);

/**
 * @brief Turn off reverb
 */
void SsUtReverbOff(void);

/**
 * @brief Set reverb depth
 *
 * @param ldepth Left depth (0-127)
 * @param rdepth Right depth (0-127)
 */
void SsUtSetReverbDepth(short ldepth, short rdepth);

/**
 * @brief Set delay time for Echo/Delay reverb
 *
 * @param delay Delay value (0-127)
 */
void SsUtSetReverbDelay(short delay);

/**
 * @brief Set feedback for Echo/Delay reverb
 *
 * @param feedback Feedback value (0-127)
 */
void SsUtSetReverbFeedback(short feedback);

/*============================================================================
 * VAB Attribute Functions
 *============================================================================*/

/**
 * @brief Get VAB attribute header
 *
 * @param vabId VAB number (0-31)
 * @param vabhdrptr Pointer to VAB header structure
 * @return 0 on success, -1 on failure
 */
short SsUtGetVabHdr(short vabId, VabHdr* vabhdrptr);

/**
 * @brief Set VAB attribute header
 *
 * Only mvol, pan, attr1, attr2 can be changed.
 *
 * @param vabId VAB number (0-31)
 * @param vabhdrptr Pointer to VAB header
 * @return 0 on success, -1 on failure
 */
short SsUtSetVabHdr(short vabId, VabHdr* vabhdrptr);

/**
 * @brief Get program attribute table
 *
 * @param vabId VAB number (0-31)
 * @param progNum Program number (0-127)
 * @param progatrptr Pointer to program attribute
 * @return 0 on success, -1 on failure
 */
short SsUtGetProgAtr(short vabId, short progNum, ProgAtr* progatrptr);

/**
 * @brief Set program attribute table
 *
 * @param vabId VAB number (0-31)
 * @param progNum Program number (0-127)
 * @param progatrptr Pointer to program attribute
 * @return 0 on success, -1 on failure
 */
short SsUtSetProgAtr(short vabId, short progNum, ProgAtr* progatrptr);

/**
 * @brief Get tone attribute table
 *
 * @param vabId VAB number (0-31)
 * @param progNum Program number (0-127)
 * @param toneNum Tone number (0-15)
 * @param vagatrptr Pointer to tone attribute
 * @return 0 on success, -1 on failure
 */
short SsUtGetVagAtr(
    short vabId, short progNum, short toneNum, VagAtr* vagatrptr);

/**
 * @brief Set tone attribute table
 *
 * @param vabId VAB number (0-31)
 * @param progNum Program number (0-127)
 * @param toneNum Tone number (0-15)
 * @param vagatrptr Pointer to tone attribute
 * @return 0 on success, -1 on failure
 */
short SsUtSetVagAtr(
    short vabId, short progNum, short toneNum, VagAtr* vagatrptr);

/**
 * @brief Get SPU buffer address stored by VAG
 *
 * @param vabId VAB data id (0-15)
 * @param vagId VAG data id (1-254)
 * @return SPU buffer address (bytes)
 */
long SsUtGetVagAddr(short vabId, short vagId);

/**
 * @brief Get SPU buffer address from tone info
 *
 * @param vabid VAB id
 * @param progid Program number
 * @param toneid Tone number
 * @return Address in sound buffer, or -1 on failure
 */
u_long SsUtGetVagAddrFromTone(short vabid, short progid, short toneid);

/**
 * @brief Get address in sound buffer for VAB data
 *
 * @param vabid VAB id
 * @return Address inside sound buffer, or -1 on failure
 */
u_long SsUtGetVBaddrInSB(short vabid);

/*============================================================================
 * Noise Functions (Not Supported)
 *============================================================================*/

/**
 * @brief Get noise clock value (not supported)
 * @return Noise clock value
 */
short SsGetNck(void);

/**
 * @brief Set noise clock value (not supported)
 * @param n_clock Noise clock (0-0x3f)
 */
void SsSetNck(short n_clock);

/**
 * @brief Turn noise off (not supported)
 */
void SsSetNoiseOff(void);

/**
 * @brief Turn noise on (not supported)
 * @param voll L channel volume
 * @param volr R channel volume
 */
void SsSetNoiseOn(short voll, short volr);

/*============================================================================
 * Debug Functions
 *============================================================================*/

/**
 * @brief Test function for low-level MIDI jump table
 *
 * Hook these dummy functions into SsFCALL to determine which
 * low-level MIDI functions are called by your sequences.
 */
void dmy_SsNoteOn(void);
void dmy_SsSetProgramChange(void);
void dmy_SsSetPitchBend(void);
void dmy_SsGetMetaEvent(void);
void dmy_SsSetControlChange(void);
void dmy_SsContBankChange(void);
void dmy_SsContDataEntry(void);
void dmy_SsContMainVol(void);
void dmy_SsContPanpot(void);
void dmy_SsContExpression(void);
void dmy_SsContDamper(void);
void dmy_SsContNrpn1(void);
void dmy_SsContNrpn2(void);
void dmy_SsContRpn1(void);
void dmy_SsContRpn2(void);
void dmy_SsContExternal(void);
void dmy_SsContResetAll(void);
void dmy_SsSetNrpnVabAttr0(void);
void dmy_SsSetNrpnVabAttr1(void);
void dmy_SsSetNrpnVabAttr2(void);
void dmy_SsSetNrpnVabAttr3(void);
void dmy_SsSetNrpnVabAttr4(void);
void dmy_SsSetNrpnVabAttr5(void);
void dmy_SsSetNrpnVabAttr6(void);
void dmy_SsSetNrpnVabAttr7(void);
void dmy_SsSetNrpnVabAttr8(void);
void dmy_SsSetNrpnVabAttr9(void);
void dmy_SsSetNrpnVabAttr10(void);
void dmy_SsSetNrpnVabAttr11(void);
void dmy_SsSetNrpnVabAttr12(void);
void dmy_SsSetNrpnVabAttr13(void);
void dmy_SsSetNrpnVabAttr14(void);
void dmy_SsSetNrpnVabAttr15(void);
void dmy_SsSetNrpnVabAttr16(void);
void dmy_SsSetNrpnVabAttr17(void);
void dmy_SsSetNrpnVabAttr18(void);
void dmy_SsSetNrpnVabAttr19(void);

/*============================================================================
 * SPU Helper Functions (from libspu, commonly used with libsnd)
 *============================================================================*/

/**
 * @brief Clear the reverb work area
 *
 * @param rev_mode Reverb mode
 * @return 0 on success
 */
long SpuClearReverbWorkArea(long rev_mode);

/**
 * @brief Get key status for all voices
 *
 * @param status Array to receive status for each voice
 */
void SpuGetAllKeysStatus(char* status);

#endif /* LIBSND_H */
