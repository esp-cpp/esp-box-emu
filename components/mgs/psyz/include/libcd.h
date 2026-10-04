#ifndef LIBCD_H
#define LIBCD_H
#include <psyz/types.h>

/**
 * @file libcd.h
 * @brief CD-ROM Library
 *
 * This library provides low-level control of the PlayStation CD-ROM drive,
 * including disc reading, audio playback, and file system access.
 *
 * Key features:
 * - CD-ROM primitive commands (play, read, seek, pause)
 * - CD-DA and CD-XA audio playback
 * - File system access with CdSearchFile()
 * - Asynchronous command execution
 * - Callback support for completion and ready events
 * - Audio volume control (CD-DA and CD-XA)
 *
 * BCD: minute/second/sector expression where each digit of a decimal number is
 * assigned to a 4-bit field. For example, decimal 60 is specified as 0x60.
 */

/* Low Level File System for CdSearchFile() */
#define CdlMAXFILE 64 /* max number of files in a directory */
#define CdlMAXDIR 128 /* max number of total directories */
#define CdlMAXLEVEL 8 /* max levels of directories */

/*
 * CD-ROM Primitive Commands
 */
#define CdlSync 0x00
#define CdlNop 0x01
#define CdlSetloc 0x02
#define CdlPlay 0x03
#define CdlForward 0x04
#define CdlBackward 0x05

/**
 * @brief  Start data read, with retry
 */
#define CdlReadN 0x06

#define CdlStandby 0x07
#define CdlStop 0x08
#define CdlPause 0x09
#define CdlReset 0x0a
#define CdlMute 0x0b
#define CdlDemute 0x0c
#define CdlSetfilter 0x0d
#define CdlSetmode 0x0e
#define CdlGetparam 0x0f
#define CdlGetlocL 0x10

/**
 * @brief Gets the physical address of the sector being read or played
 *
 * Gets the physical address of the sector being read or played. The table below
 * shows the obtainable parameters. CdlGetlocP gets the subcode address, so it
 * is effective on all sector types, including audio sectors.
 *
 *  | Result byte no. | Details
 *  |-----------------|----------------------------
 *  | 0               | Track number (BCD)
 *  | 1               | Index number (BCD)
 *  | 2               | Track relative minute (BCD)
 *  | 3               | Track relative second (BCD)
 *  | 4               | Track relative sector (BCD)
 *  | 5               | Absolute minute (BCD)
 *  | 6               | Absolute second (BCD)
 *  | 7               | Absolute sector (BCD)
 * Track relative minute/second/sector indicates an offset value from that
 * track's header location. Absolute minute/second/sector specifies the location
 * from the initial track.
 */
#define CdlGetlocP 0x11

/**
 * @brief Obtains number of TOC entries
 *
 * @return 0: Status, 1: Intial track Number (BCD), 2: Final track Number (BCD)
 */
#define CdlGetTN 0x13

/**
 * @brief Obtains TOC entry for track
 *
 * Obtains the TOC entries information (min, sec) corresponding to the
 * track number specified in the parameters.
 * Please set the track No. in the BCD parameters.
 *
 * @return 0: Status, 1: TOC min, 2: TOC sec
 */
#define CdlGetTD 0x14

#define CdlSeekL 0x15
#define CdlSeekP 0x16

/**
* @brief  Commence data read, no retry
 */
#define CdlReadS 0x1B

/*
 * CD-ROM Mode (used in CdlSetmode)
 */
#define CdlModeStream 0x100  /**< Normal Streaming */
#define CdlModeStream2 0x120 /**< SUB HEADER information includes */
#define CdlModeSpeed 0x80    /**< normal speed: 0, double speed: 1 */
#define CdlModeRT 0x40       /**< 0: ADPCM off, 1: ADPCM on */
#define CdlModeSize1 0x20    /**< 0: 2048 byte, 1: 2340byte */
#define CdlModeSize0 0x10    /**< 0: -, 1: 2328byte */
#define CdlModeSF 0x08       /**< 0: Channel off, 1: Channel on */
#define CdlModeRept 0x04     /**< 0: Report off, 1: Report on */
#define CdlModeAP 0x02       /**< 0: AutoPause off, 1: AutoPause on */
#define CdlModeDA 0x01       /**< 0: CD-DA off, 1: CD-DA on */

#define CdlModeSpeedNormal 0
#define CdlModeSpeedDouble 1

/*
 * Status Contents
 */
#define CdlStatPlay 0x80      /* playing CD-DA */
#define CdlStatSeek 0x40      /* seeking */
#define CdlStatRead 0x20      /* reading data sectors */
#define CdlStatShellOpen 0x10 /* once shell open */
#define CdlStatSeekError 0x04 /* seek error detected */
#define CdlStatStandby 0x02   /* spindle motor rotating */
#define CdlStatError 0x01     /* command error detected */

/*
 * Interrupts
 */
#define CdlNoIntr 0x00      /* No interrupt */
#define CdlDataReady 0x01   /* Data Ready */
#define CdlComplete 0x02    /* Command Complete */
#define CdlAcknowledge 0x03 /* Acknowledge (reserved) */
#define CdlDataEnd 0x04     /* End of Data Detected */
#define CdlDiskError 0x05   /* Error Detected */

/*
 * Library Macros
 */
#define btoi(b) ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define itob(i) ((i) / 10 * 16 + (i) % 10) /* u_char to BCD */

#define CdSeekL(p) CdControl(CdlSeekL, (u_char*)p, 0)
#define CdSeekP(p) CdControl(CdlSeekP, (u_char*)p, 0)
#define CdStandby() CdControl(CdlStandby, 0, 0)
#define CdPause() CdControl(CdlPause, 0, 0)
#define CdStop() CdControl(CdlStop, 0, 0)
#define CdMute() CdControl(CdlMute, 0, 0)
#define CdDeMute() CdControl(CdlDemute, 0, 0)
#define CdForward() CdControl(CdlForward, 0, 0)
#define CdBackward() CdControl(CdlBackward, 0, 0)

/*
 *	Position
 */
#define CdlMAXTOC 100

/**
 * @brief Time-code based CD-ROM disc position
 *
 * Defines a time-code position on a CD-ROM, based on the time needed to
 * reach that position when playing the disc from the beginning at normal
 * speed. The track member is not used at present.
 */
typedef struct {
    u_char minute; /* minute (BCD) */
    u_char second; /* second (BCD) */
    u_char sector; /* sector (BCD) */
    u_char track;  /* track (void) */
} CdlLOC;

/**
 * @brief Audio attenuation structure
 *
 * Sets CD audio volume (consisting of CD-DA audio and CD-XA audio).
 * Val0 - val3 can range from 0 (minimum volume) to 128 (maximum volume).
 * For adjusting normal stereo volume, set the L channel volume in val0 and the
 * R channel volume in val2. Val1 and val3 should be set to 0.
 */
typedef struct {
    u_char val0; /* volume for CD(L) -> SPU (L) */
    u_char val1; /* volume for CD(L) -> SPU (R) */
    u_char val2; /* volume for CD(R) -> SPU (L) */
    u_char val3; /* volume for CD(R) -> SPU (R) */
} CdlATV;

/**
 * @brief CD-ROM file and channel filter
 *
 * Used with CdlSetfilter command to filter sectors by file number and/or
 * channel number during CD-XA streaming.
 */
typedef struct {
    u_char file; /**< File number (0-31, 0xff = no filter) */
    u_char chan; /**< Channel number (0-31, 0xff = no filter) */
    u_short pad; /**< Padding */
} CdlFILTER;

typedef struct {
    u_short id;
    u_short type;
    u_short secCount;
    u_short nSectors;
    u_long frameCount;
    u_long frameSize;

    u_short width;
    u_short height;
    u_long dummy1;
    u_long dummy2;
    CdlLOC loc;
} StHEADER;

typedef struct {
    CdlLOC pos;    /* file location */
    int size;      /* file size */
    char name[16]; /* file name (body) */
} CdlFILE;

/**
 * @brief CD-ROM callback function type
 */
typedef void (*CdlCB)(u_char, u_char*);

void def_cbsync(u_char intr, u_char* result);
void def_cbready(u_char intr, u_char* result);
void def_cbread(u_char intr, u_char* result);

/**
 * @brief Initialize CD-ROM system
 *
 * Initializes the CD-ROM system and the low-level file system that uses it.
 * Call this function before using any other CD-ROM functions.
 *
 * @return 1 if successful, 0 otherwise
 */
int CdInit(void);

/**
 * @brief Get CD-ROM status
 *
 * Returns the status of the CD-ROM system.
 *
 * @return Current status value
 */
int CdStatus(void);

/**
 * @brief Get current mode
 *
 * Returns the current mode setting.
 *
 * @return Current mode value
 */
int CdMode(void);

/**
 * @brief Get last command
 *
 * Returns the last command that was issued.
 *
 * @return Last command code
 */
int CdLastCom(void);

/**
 * @brief Get last seek position
 *
 * Returns a pointer to the last seek position.
 *
 * @return Pointer to CdlLOC structure containing last position
 */
CdlLOC* CdLastPos(void);

/**
 * @brief Reset CD-ROM system
 *
 * Resets the CD-ROM system.
 *
 * @param mode Reset mode
 * @return 1 if successful, 0 otherwise
 */
int CdReset(int mode);

/**
 * @brief Flush read buffer
 *
 * Flushes the CD-ROM read buffer.
 */
void CdFlush(void);

/**
 * @brief Set debug level
 *
 * Sets the debug output level for CD-ROM operations.
 *
 * @param level Debug level (0 = off)
 * @return Previous debug level
 */
int CdSetDebug(int level);

/**
 * @brief Get character string corresponding to command code (for debugging)
 *
 * For debugging. Get corresponding character string from processing status
 * code. Example: CdlNop returns "CdlNop", CdlSetloc returns "CdlSetloc", and so
 * on.
 *
 * @param com Command completion code
 * @return Pointer to start of character string
 */
char* CdComstr(u_char com);

/**
 * @brief Get character string corresponding to interrupt code (for debugging)
 *
 * For debugging. Returns a character string corresponding to the interrupt
 * code.
 *
 * @param intr Interrupt code
 * @return Pointer to character string
 */
char* CdIntstr(u_char intr);

/**
 * @brief Wait for command completion
 *
 * Waits for the current CD-ROM command to complete.
 *
 * @param mode 0 = wait for completion, 1 = check status
 * @param result Pointer to result buffer (8 bytes)
 * @return Command completion status
 */
int CdSync(int mode, u_char* result);

/**
 * @brief Wait until drive is ready
 *
 * Waits until the CD-ROM drive is ready.
 *
 * @param mode 0 = wait for ready, 1 = check status
 * @param result Pointer to result buffer
 * @return Ready status
 */
int CdReady(int mode, u_char* result);

/**
 * @brief Set sync callback function
 *
 * Sets the callback function to be called when a command completes.
 *
 * @param func Callback function pointer
 * @return Previous callback function
 */
CdlCB CdSyncCallback(CdlCB func);

/**
 * @brief Set ready callback function
 *
 * Sets the callback function to be called when the drive becomes ready.
 *
 * @param func Callback function pointer
 * @return Previous callback function
 */
CdlCB CdReadyCallback(CdlCB func);

/**
 * @brief Issue primitive command to CD-ROM system
 *
 * Issues the primitive command com to the CD-ROM system. param points to the
 * arguments of the command, if any; set param to 0 for commands that do not
 * require arguments. result points to a buffer used to hold the return value;
 * if result is 0, the return value is not stored.
 *
 * @param com Command code
 * @param param Pointer to command arguments
 * @param result Pointer to return value storage buffer (requires 8 bytes)
 * @return non-zero if successful, 0 otherwise
 */
int CdControl(u_char com, u_char* param, u_char* result);

/**
 * @brief Issue blocking primitive command
 *
 * Same as CdControl() but blocks until command completes.
 *
 * @param com Command code
 * @param param Pointer to command arguments
 * @param result Pointer to return value storage buffer
 * @return non-zero if successful, 0 otherwise
 */
int CdControlB(u_char com, u_char* param, u_char* result);

/**
 * @brief Issue fast primitive command
 *
 * Fast version of CdControl() that doesn't wait for result.
 *
 * @param com Command code
 * @param param Pointer to command arguments
 * @return non-zero if successful, 0 otherwise
 */
int CdControlF(u_char com, u_char* param);

/**
 * @brief Set audio volume
 *
 * Sets the audio volume for CD audio (CD-DA, ADPCM).
 *
 * @param vol Pointer to attenuator volume
 * @return 1
 */
int CdMix(CdlATV* vol);

/**
 * @brief Get sector data
 *
 * Retrieves sector data from the read buffer.
 *
 * @param madr Destination address
 * @param size Size of data to retrieve
 * @return Number of bytes retrieved
 */
int CdGetSector(void* madr, int size);

/**
 * @brief Read CD-ROM table of contents information
 *
 * Get starting position of each track on the CD-ROM disc.
 * The maximum number of tracks is 100.
 *
 * @param loc Pointer to location table
 * @return Positive integer is a track number; anything else is an error
 */
int CdGetToc(CdlLOC* loc);

/**
 * @brief Set data ready callback
 *
 * Sets the callback function to be called when data is ready.
 *
 * @param func Callback function pointer
 * @return Previous callback function
 */
CdlCB CdDataCallback(CdlCB func);

/**
 * @brief Convert integer to position
 *
 * Converts an integer sector number to a CdlLOC position.
 *
 * @param i Sector number
 * @param p Pointer to CdlLOC structure
 * @return Pointer to p
 */
CdlLOC* CdIntToPos(int i, CdlLOC* p);

/**
 * @brief Convert position to integer
 *
 * Converts a CdlLOC position to an integer sector number.
 *
 * @param p Pointer to CdlLOC structure
 * @return Sector number
 */
int CdPosToInt(CdlLOC* p);

/**
 * @brief Search for file on CD
 *
 * Searches for a file in the ISO-9660 file system.
 *
 * @param fp Pointer to CdlFILE structure to receive file info
 * @param name File name to search for
 * @return Pointer to fp if found, NULL otherwise
 */
CdlFILE* CdSearchFile(CdlFILE* fp, char* name);

/**
 * @brief Read sectors from CD
 *
 * Initiates reading of sectors from the CD.
 *
 * @param sectors Number of sectors to read
 * @param buf Pointer to destination buffer
 * @param mode Read mode
 * @return 1 if successful, 0 otherwise
 */
int CdRead(int sectors, u_long* buf, int mode);

/**
 * @brief Wait for read completion
 *
 * Waits for the current read operation to complete.
 *
 * @param mode 0 = wait for completion, 1 = check status
 * @param result Pointer to result buffer
 * @return Read completion status
 */
int CdReadSync(int mode, u_char* result);

/**
 * @brief Set read callback function
 *
 * Sets the callback function to be called when read completes.
 *
 * @param func Callback function pointer
 * @return Previous callback function
 */
CdlCB CdReadCallback(CdlCB func);

/**
 * @brief Alternate read function
 *
 * Alternate version of CdRead().
 *
 * @param mode Read mode
 * @return 1 if successful, 0 otherwise
 */
int CdRead2(long mode);

/**
 * @brief Clear streaming ring buffer
 *
 * Clears the streaming ring buffer.
 */
void StClearRing(void);

/**
 * @brief Set streaming parameters
 *
 * Sets parameters for streaming playback.
 *
 * @param mode Streaming mode
 * @param start_frame Starting frame number
 * @param end_frame Ending frame number
 * @param func1 Callback function 1
 * @param func2 Callback function 2
 */
void StSetStream(u_long mode, u_long start_frame, u_long end_frame,
                 void (*func1)(), void (*func2)());

/**
 * @brief Set streaming mask
 *
 * Sets mask for streaming operations.
 *
 * @param mask Mask value
 * @param start Start value
 * @param end End value
 */
void StSetMask(u_long mask, u_long start, u_long end);

/**
 * @brief Get next streaming frame
 *
 * Gets the next frame from the streaming buffer.
 *
 * @param addr Pointer to receive frame address
 * @param header Pointer to receive header address
 * @return Frame status
 */
u_long StGetNext(u_long** addr, u_long** header);

/**
 * @brief Free streaming ring buffer
 *
 * Frees space in the streaming ring buffer.
 *
 * @param base Base address
 * @return Amount freed
 */
u_long StFreeRing(u_long* base);

/**
 * @brief Get back location
 *
 * Gets the back location in streaming buffer.
 *
 * @param loc Pointer to CdlLOC structure
 * @return 1 if successful, 0 otherwise
 */
int StGetBackloc(CdlLOC* loc);

/**
 * @brief Set streaming ring buffer
 *
 * Sets up the streaming ring buffer.
 *
 * @param ring_addr Ring buffer address
 * @param ring_size1 Ring buffer size
 */
void StSetRing(u_long* ring_addr, u_long ring_size1);

/**
 * @brief Unset streaming ring buffer
 *
 * Removes the streaming ring buffer.
 */
void StUnSetRing(void);

#endif
