#ifndef LIBDS_H
#define LIBDS_H
#include <psyz/types.h>

/**
 * @file libds.h
 * @brief CD-ROM Extended Library
 *
 * This library provides high-level CD-ROM functions with a simpler interface
 * than libcd. It offers asynchronous streaming, automatic data transfer, and
 * built-in error handling.
 *
 * Key features:
 * - Simplified command interface (DsRead, DsPlay, DsSeek)
 * - Automatic sector buffering and data streaming
 * - Ready callbacks for data arrival notification
 * - Command queue management (up to 32 commands)
 * - Disk type detection and system status monitoring
 * - Support for CD-DA, CD-XA, and data reading
 *
 * Note: Cannot be used together with libcd.
 */

/* Callback function types */

/**
 * @brief CD-ROM callback function type
 */
typedef void (*DslCB)(unsigned char, unsigned char*);

/**
 * @brief Ready callback function type
 */
typedef void (*DslRCB)(unsigned char, unsigned char*);

/* Return value constants */

/* DsGetDiskType return values */
#define DslCdromFormat 0   /**< PlayStation disk */
#define DslOtherFormat 1   /**< Any other type of CD */
#define DslStatNoDisk 2    /**< CD is not installed */
#define DslStatShellOpen 3 /**< CD cover is open */

/* DsReady return values */
#define DslDataReady 1 /**< New data has arrived in the sector buffer */
#define DslNoIntr 0    /**< No interrupt / new data has not arrived */
#define DslDataEnd 2   /**< Final sector confirmed (DA playback only) */

/* DsSync return values */
#define DslComplete 2  /**< Command has terminated normally */
#define DslDiskError 5 /**< Command failed / Disk error */
#define DslNoResult -1 /**< Execution terminated but results destroyed */

/* DsSystemStatus return values */
#define DslReady 0 /**< No command is being executed */
#define DslBusy 1  /**< Command is being executed */
#define DslNoCD 2  /**< No CD is installed */

/* System constants */
#define DslMaxCOMMANDS 32 /**< Maximum number of commands in queue */
#define DslMaxRESULTS 32  /**< Maximum number of saved results */

/* Structures */

/**
 * @brief Audio attenuator
 *
 * Structure for setting the CD volume (CD-DA and CD-XA).
 * The values for val0-val3 can range from 0 to 128.
 * For standard stereo volume adjustments:
 * - val0 is set to the L channel volume
 * - val1 is set to 0
 * - val2 is set to the R channel volume
 * - val3 is set to 0
 */
typedef struct {
    u_char val0; /**< CD (L) -> SPU (L) attenuation */
    u_char val1; /**< CD (L) -> SPU (R) attenuation */
    u_char val2; /**< CD (R) -> SPU (R) attenuation */
    u_char val3; /**< CD (R) -> SPU (L) attenuation */
} DslATV;

/**
 * @brief CD-ROM location
 *
 * Specifies the CD-ROM position. Each element is specified using BCD.
 */
typedef struct {
    u_char minute; /**< Minutes */
    u_char second; /**< Seconds */
    u_char sector; /**< Sectors */
    u_char track;  /**< Track number (currently unused) */
} DslLOC;

/**
 * @brief ISO9660 file descriptor
 *
 * Stores the position and size of a type 9660 CD-ROM file.
 */
typedef struct {
    DslLOC pos;    /**< File position */
    u_long size;   /**< File size (in bytes) */
    char name[16]; /**< Filename */
} DslFILE;

/**
 * @brief ADPCM channel
 *
 * Specifies the ADPCM sector channel to be played back.
 */
typedef struct {
    u_char file; /**< File number */
    u_char chan; /**< Channel number */
    u_short pad; /**< Reserved for system use */
} DslFILTER;

/* Functions */

/**
 * @brief Close the libds system
 *
 * Closes the libds system, resets the libds kernel state machine, and detaches
 * the callback function which controls the libds kernel. This function must be
 * called whenever control is passed to a child process, LoadExec() is
 * performed, or when CD control functions outside of libds are used. Call
 * DsInit() to reopen libds.
 */
void DsClose(void);

/**
 * @brief Add primitive command to the command queue
 *
 * Adds a command to the queue to be performed in the background. If execution
 * of the command fails, it is retried count times. Separate callback functions
 * can be set for each command. The execution status of a command can be
 * obtained with DsSync().
 *
 * @param com Command code
 * @param param Pointer to argument for command (u_char[4])
 * @param cbsync Pointer to callback function
 * @param count Number of retries (0: no retries, -1: unlimited retries)
 * @return The command ID (>0) if the command issued successfully, otherwise 0
 */
int DsCommand(u_char com, u_char* param, DslCB cbsync, int count);

/**
 * @brief Get the character string corresponding to each command code
 *
 * Gets the corresponding character string from the process status code (used
 * for debugging). For example, DslNop returns "DslNop", DslSetloc returns
 * "DslSetLoc", and so forth.
 *
 * @param com Command completion code
 * @return Pointer to start of character string
 */
char* DsComstr(u_char com);

/**
 * @brief CdControl() compatibility function
 *
 * Provides the same interface as CdControl(). Unlike CdControl(), however, the
 * command is handled such that the function blocks until the end of the
 * operation, even if the command itself is non-blocking.
 *
 * @param com Command code
 * @param param Pointer to arguments for command (u_char[4])
 * @param result Pointer to storage for the return value (u_char[8])
 * @return 1 if execution of command was successful, 0 if failed
 */
int DsControl(u_char com, u_char* param, u_char* result);

/**
 * @brief CdControlB() compatibility function
 *
 * Provides the same interface as CdControlB(). The actual timing differs
 * somewhat since the command queue is used.
 *
 * @param com Command code
 * @param param Arguments for command (u_char[4])
 * @param result Return value for command (u_char[8])
 * @return 1 if execution of command was successful, 0 if failed
 */
int DsControlB(u_char com, u_char* param, u_char* result);

/**
 * @brief CdControlF() compatibility function
 *
 * Provides the same interface as CdControlF() except for a few differences
 * such as timing. Internally, the specified command is simply added to the
 * command queue. CdControlF() waits for the previous command to complete
 * execution before issuing the new command. DsControlF(), on the other hand,
 * adds the new command to the queue if the previous command has not completed.
 *
 * @param com Command code
 * @param param Pointer to arguments for command (u_char[4])
 * @return The command ID (>0) if the command was successfully added to the
 * queue; 0 otherwise
 */
int DsControlF(u_char com, u_char* param);

/**
 * @brief Set exit callback for DsGetSector() and DsGetSector2()
 *
 * Defines func as the callback to be executed on completion of a read operation
 * initiated by DsGetSector() or DsGetSector2(). No callback is generated when
 * func is set to 0. This callback is really only useful with DsGetSector2(),
 * since the transfer of data is finished when DsGetSector() exits.
 *
 * @param func Pointer to callback function
 * @return Pointer to previous callback
 */
void (*DsDataCallback(void (*func)()))(void);

/**
 * @brief Wait for completion of DsGetSector2
 *
 * Waits for the transfer performed by DsGetSector2() to complete.
 *
 * @param mode 0: Wait for end of transfer, 1: Check current status and return
 * immediately
 * @return 1 if transfer is in progress, 0 if transfer is complete
 */
int DsDataSync(int mode);

/**
 * @brief End simple callback system
 *
 * Ends simple callback system. This function is executed within the callback
 * function provided to DsStartReadySystem().
 */
void DsEndReadySystem(void);

/**
 * @brief Flush the command queue
 *
 * All commands that have been entered in the command queue are flushed.
 * Currently executing commands are allowed to complete, but the results are
 * not saved and callbacks are not invoked. If a command is executing when this
 * function is called, it is allowed to complete. Subsequent commands are put
 * into a new queue.
 */
void DsFlush(void);

/**
 * @brief Get CD type
 *
 * Gets the type of CD currently installed: either a PlayStation (black) or
 * non-PlayStation disk. This function blocks until the system status changes
 * to DslReady. The debugging station recognizes ISO9660 CDs (including CD-Rs)
 * as type DslCdromFormat. This function does not operate properly on the
 * DTL-H2000. This function internally changes the operation mode to
 * DslModeSize1(0x20) when complete, so the mode must be reset as necessary.
 *
 * @return DslCdromFormat (PlayStation disk), DslOtherFormat (other CD),
 *         DslStatNoDisk (no CD), or DslStatShellOpen (cover open)
 */
int DsGetDiskType(void);

/**
 * @brief Transfer data from the sector buffer to main memory
 *
 * Data is transferred from the sector buffer to the storage area in main
 * memory pointed to by madr. This function blocks until the end of the transfer
 * operation. The sector size varies according to the mode. The data from the
 * sector buffer can be transferred to memory over a number of iterations. The
 * transfer is complete when the function returns.
 *
 * @param madr Pointer to destination area in main memory
 * @param size Transfer size (long word)
 * @return Always returns 1
 */
int DsGetSector(void* madr, int size);

/**
 * @brief Transfer data from the sector buffer to main memory (non-blocking)
 *
 * Data is transferred from the sector buffer to the storage area in main
 * memory pointed to by madr. The transfer is performed in cycle-stealing mode
 * so interrupts may be received during the transfer. Since DsGetSector2() is a
 * non-blocking function that can return after the transfer starts, the
 * completion of transfer must be determined using DsDataSync() or
 * DsDataCallback(). Data transfers in cycle-stealing mode are more
 * time-consuming compared to blocking mode.
 *
 * @param madr Pointer to destination area in main memory
 * @param size Transfer size (long word)
 * @return Always returns 1
 */
int DsGetSector2(void* madr, int size);

/**
 * @brief TOC read
 *
 * The starting position of each track on the CD-ROM is obtained. The largest
 * track number is 100.
 *
 * @param loc Location table
 * @return Positive integer: track number; Other values: error
 */
int DsGetToc(DslLOC* loc);

/**
 * @brief Perform system initialization
 *
 * Initializes the libds system. DsInit() needs to be called just once at the
 * beginning of a program or when restarting a system that was stopped with
 * DsClose(). Calling DsInit() in the middle of a program may cause improper
 * operation. DsReset() should be used if initialization needs to be performed
 * in the middle of a program. Because DsInit() resets the SPU sound volume and
 * the CD input volume to SPU, etc., it should either be called before libspu
 * and libsnd initialization/setting functions or they should be reset after
 * DsInit() is called.
 *
 * @return 1 if successful; 0 if the operation failed
 */
int DsInit(void);

/**
 * @brief Get the corresponding character string for the command process status
 *
 * For debugging. Gets the corresponding character string from the process
 * status code. DslNoIntr returns "NoIntr", DslComplete returns "Complete",
 * DslDiskError returns "Disk Error".
 *
 * @param intr Execution status code
 * @return Pointer to start of character string
 */
char* DsInstr(u_char intr);

/**
 * @brief Get minutes, seconds, sectors from absolute sector number
 *
 * The absolute sector number specified by i is converted to minutes, seconds,
 * and sectors and the result is stored in the DslLOC structure pointed to by p.
 *
 * @param i Absolute sector number
 * @param p Pointer to buffer for storing result
 * @return Pointer to result buffer
 */
DslLOC* DsIntToPos(int i, DslLOC* p);

/**
 * @brief Get the command issued last
 *
 * Returns the primitive command code issued last. However, because this
 * function is replaced by DslNop in libds in certain situations, the desired
 * value may not be returned.
 *
 * @return Primitive command code
 */
u_char DsLastCom(void);

/**
 * @brief Get the last setloc position
 *
 * The last setloc position is obtained and the result is stored in the DslLOC
 * structure pointed to by p.
 *
 * @param p Pointer to buffer in which position is stored
 * @return Pointer to result buffer
 */
DslLOC* DsLastPos(DslLOC* p);

/**
 * @brief Set attenuator
 *
 * The CD audio volume (CD-DA/ADPCM) is set to the value in the DslATV
 * structure pointed to by vol.
 *
 * @param vol Attenuator volume
 * @return 1
 */
int DsMix(DslATV* vol);

/**
 * @brief Add a sequence of commands to the queue
 *
 * Adds a sequence of commands that perform a data read (playback) to the queue.
 * The commands added are: DslPause; DslSetmode mode; DslSetloc pos; com.
 * If any command in the sequence generates an error, a retry is performed
 * starting with the first command, up to a maximum of count times. An error is
 * generated if the queue does not have enough space for the command sequence.
 *
 * @param mode Operating mode
 * @param pos Pointer to DslLOC structure specifying target position
 * @param com Last command to be executed (DslPlay, DslReadN, DslReadS,
 * DslSeekP, DslSeekL)
 * @param cbsync Callback function triggered when all commands have been
 * executed
 * @param count Retry count (0: no retries, -1: unlimited retries)
 * @return The command ID (>0) if the command was added to the queue; 0 if
 * failed
 */
int DsPacket(u_char mode, DslLOC* pos, u_char com, DslCB cbsync, int count);

/**
 * @brief Play back CD-DA tracks
 *
 * The tracks specified by the tracks array are played in sequence in the
 * background. When the final track in the series is done, playback is repeated
 * or is stopped, depending on mode. Mode values: 0=Stop playback, 1=Play then
 * stop, 2=Play then repeat, 3=Return index of track currently being played.
 * Playback is performed in increments of tracks.
 *
 * @param mode Mode
 * @param tracks Pointer to array specifying the tracks to be played back; the
 * last element must be 0
 * @param offset Index for track to begin playback
 * @return The track currently being played (index in tracks array); -1 means
 * all tracks have finished
 */
int DsPlay(int mode, int* tracks, int offset);

/**
 * @brief Get absolute sector number from minutes, seconds, sectors
 *
 * Calculates the absolute sector number from the minutes, seconds, and sectors
 * in the DslLOC structure pointed to by p.
 *
 * @param p Pointer to DslLOC structure containing minutes, seconds, sectors
 * @return The absolute sector number
 */
int DsPosToInt(DslLOC* p);

/**
 * @brief Get the number of commands stored in the command queue
 *
 * Obtains the number of primitive commands stored in the command queue. The
 * commands issued by DsPacket() are not removed from the queue until they all
 * successfully complete. The currently executing command is considered to be in
 * the queue. The maximum number of commands that the queue can hold is defined
 * by the DslMaxCOMMANDS macro constant.
 *
 * @return Number of commands in queue
 */
int DsQueueLen(void);

/**
 * @brief Read data
 *
 * Reads CD data starting at the location specified by the DslLOC structure
 * pointed to by pos. The data is stored in the buffer pointed to by buf. The
 * operation is performed in sectors, so the size of buf must be a multiple of
 * 1 sector=2048 bytes (512 words). Reading is performed in the background after
 * DsRead() has executed and exited.
 *
 * @param pos Pointer to DslLOC structure specifying starting position of CD
 * @param sectors Number of sectors to read
 * @param buf Pointer to buffer to store read data
 * @param mode Operating mode to be used when data is being read
 * @return Positive integer: the id of the packet that was issued; 0: execution
 * failed
 */
int DsRead(DslLOC* pos, int sectors, u_long* buf, int mode);

/**
 * @brief Begin playback of movie
 *
 * Plays back the movie starting at the location specified by the DslLOC
 * structure pointed to by pos. A libds streaming library callback is set and
 * the reading of data is begun with DslReadS.
 *
 * @param pos Pointer to DslLOC structure specifying starting position of CD
 * @param mode Operating mode during playback
 * @return The command ID (>0) if the function succeeded; 0 if the command
 * failed
 */
int DsRead2(DslLOC* pos, int mode);

/**
 * @brief Interrupt DsRead() operation
 *
 * Interrupts a DsRead() operation.
 */
void DsReadBreak(void);

/**
 * @brief Set a callback function to be called when DsRead() is finished
 *
 * Defines func as the callback to be triggered when DsRead() completes.
 *
 * @param func Pointer to callback function
 * @return Pointer to previous callback function
 */
DslCB DsReadCallback(DslCB func);

/**
 * @brief Read an executable file
 *
 * Loads the executable file specified by file from the CD-ROM and stores it in
 * main memory. It is a blocking function. The loaded file is executed as a
 * child process using Exec(). The load address of the executable file must not
 * overlap with the area used by the parent process.
 *
 * @param file Filename
 * @return Pointer to EXEC structure of the loaded executable file
 */
struct EXEC* DsReadExec(char* file);

/**
 * @brief Read a file from CD-ROM
 *
 * Reads nbyte bytes from the CD-ROM file specified by file and stores them at
 * the buffer pointed to by addr. nbyte must be a multiple of 2048; if it is 0,
 * the entire file is read. If file is NULL, the read operation begins from the
 * point where the previous DsReadFile() left off. The filenames must all be
 * represented by absolute paths. Lowercase characters are automatically
 * converted to uppercase. Although the read is performed in the background,
 * DsSearchFile() is called internally before the read begins, so it is blocked
 * for that period.
 *
 * @param file Filename
 * @param addr Pointer to buffer in memory for storing read data
 * @param nbyte Number of bytes to read (multiple of 2048, or 0 for entire file)
 * @return The number of bytes read, or 0 if an error occurred
 */
int DsReadFile(char* file, u_long* addr, int nbyte);

/**
 * @brief Wait for completion of DsRead()
 *
 * Waits for completion of DsRead(). Returns the execution status of DsRead() at
 * the point when DsReadSync() was called.
 *
 * @param result Pointer to buffer holding execution results (u_char[8])
 * @return Positive integer: the remaining number of sectors; 0: DsRead() has
 * completed; -1: an error was detected (DsRead() was interrupted)
 */
int DsReadSync(u_char* result);

/**
 * @brief Check for arrival of data
 *
 * Determines the status of a data read operation (DslReadS/DslReadN) and stores
 * the result in the buffer pointed to by result. In report mode, DsReady()
 * checks for arrival of the report from DA playback. The sector buffer value is
 * meaningful only for data reads.
 *
 * @param result Pointer to buffer for storing results (u_char[8])
 * @return DslDataReady (new data arrived), DslNoIntr (no new data), or
 * DslDataEnd (final sector confirmed, DA playback only)
 */
int DsReady(u_char* result);

/**
 * @brief Set up Ready callback function
 *
 * Sets the Ready callback to the function pointed to by func. It is called for
 * data ready interrupts, data end interrupts (generated only for DA playback),
 * and all error interrupts.
 *
 * @param func Pointer to callback function
 * @return Pointer to previous callback function
 */
DslCB DsReadyCallback(DslCB func);

/**
 * @brief Set action of cover open/close for the simple callback
 *
 * Sets the action of cover open/close for the simple callback. When mode=0, if
 * the cover is opened during execution, stop processing and end the simple
 * callback. When mode=1, if the cover is opened or closed, reissue the command
 * with an error and continue processing. Initial value is 0.
 *
 * @param mode 0: When cover is open, end simple callback; 1: After cover
 * opens/closes, perform automatic retry
 * @return Previously updated mode
 */
int DsReadySystemMode(int mode);

/**
 * @brief Reset system
 *
 * Resets the libds system. Always use DsReset() when initializing the system in
 * the middle of a program. DsInit() cannot be used in the middle of a program.
 *
 * @return 1 if the reset was successful, 0 otherwise
 */
int DsReset(void);

/**
 * @brief Get position and size of CD-ROM file
 *
 * Obtains the absolute position (minutes, seconds, sectors) and size of the
 * CD-ROM file specified by name and stores the result in the DslFILE structure
 * pointed to by fp. Filenames must be represented by their absolute paths. The
 * position data for all the files in the same directory as the file specified
 * by fp is cached in memory. Therefore, when DsSearchFile() is performed
 * consecutively for files from a single directory, access is faster from the
 * second file on.
 *
 * @param fp Pointer to CD-ROM file structure
 * @param name Filename
 * @return 0 (file not found), -1 (read operation on directory failed), or
 * pointer to the retrieved file structure
 */
DslFILE* DsSearchFile(DslFILE* fp, char* name);

/**
 * @brief Set the debug level
 *
 * Sets the debug level for the CD-ROM subsystem. 0: Do not perform any checks;
 * 1: Check primitive commands.
 *
 * @param level Debug level
 * @return Previous debug level
 */
int DsSetDebug(int level);

/**
 * @brief Get the number of times the cover was opened
 *
 * Obtains the number of times the cover was opened since the program began. The
 * count is initialized to 1 when the program starts. This function returns the
 * correct value only when DsSystemStatus()=DslReady.
 *
 * @return Number of times the cover was opened
 */
int DsShellOpen(void);

/**
 * @brief Start the simple callback
 *
 * Starts the simple callback. When the simple callback is started, a
 * DslDiskError results in a retry of the last command. count is the total
 * number of retries from the point when the system is started. The callback
 * function func normally triggers when a data read successfully completes. This
 * function is always executed from a callback from a corresponding data read
 * (playback) command. DsReadyCallback() should be used internally for simple
 * callback.
 *
 * @param func Pointer to callback function
 * @param count Retry count (-1: unlimited retries)
 * @return 1 if the function was successful; 0 if failed (system already
 * started)
 */
int DsStartReadySystem(DslRCB func, int count);

/**
 * @brief Get the status of the CD subsystem
 *
 * Obtains the last reported status of the CD subsystem. Because updating of the
 * status can sometimes be delayed, in rare cases the value may be different
 * from the current CD subsystem status. To wait for any delays to pass, use
 * DslNop to get the most recent status.
 *
 * @return Status of the CD subsystem
 */
u_char DsStatus(void);

/**
 * @brief Check for completion of primitive command
 *
 * Obtains the execution status of the primitive command specified by id and
 * stores it in the memory area pointed to by result. The execution status
 * refers to the command corresponding to the command ID that was active when
 * the function was called. result is valid only for the return values of
 * DslComplete or DslDiskError. If id is set to 0, the most current result
 * regardless of the type of command can be obtained. A certain number of
 * execution results from commands are saved (max defined by DslMaxRESULTS).
 *
 * @param id Command ID
 * @param result Pointer to buffer for storing result (u_char[8])
 * @return DslComplete (command terminated normally), DslDiskError (command
 * failed), DslNoIntr (command not yet executed), or DslNoResult (execution
 * terminated but results destroyed)
 */
int DsSync(int id, u_char* result);

/**
 * @brief Set Sync Callback function
 *
 * Defines func as the Sync callback to be triggered for all command termination
 * and error interrupts. If the queue performs retries for commands that
 * generate errors, the Sync callback function is triggered after each failure
 * (rather than the individual callback function set for the command itself).
 *
 * @param func Pointer to callback function
 * @return Pointer to previous callback function
 */
DslCB DsSyncCallback(DslCB func);

/**
 * @brief Get status of command queue
 *
 * Returns the status of the command queue. Commands issued when the status is
 * not DslReady are all added to the queue; otherwise, the command is executed
 * immediately.
 *
 * @return DslReady (no command being executed), DslBusy (command being executed
 * or cannot be executed), or DslNoCD (no CD installed)
 */
int DsSystemStatus(void);

#endif
