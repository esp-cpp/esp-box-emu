#ifndef LIBMCRD_H
#define LIBMCRD_H
#include <psyz/types.h>

/**
 * @file libmcrd.h
 * @brief Extended Memory Card Library
 *
 * This library provides high-level file access functions for PlayStation Memory
 * Cards. It offers a more convenient interface than the low-level libcard
 * functions, with support for asynchronous operations, file management, and
 * error handling.
 *
 * Key features:
 * - File creation, deletion, reading, and writing
 * - Asynchronous operations with callbacks
 * - Memory Card detection and status checking
 * - Formatting and directory access
 * - Support for both blocking and non-blocking operations
 *
 * Port numbers:
 * - Port A: 0x00 (direct connect)
 * - Port B: 0x10 (direct connect)
 * - Card number: normally 0 (supports multitap expansion)
 */

/* Forward declaration from libapi.h */
#ifndef DIRENTRY
struct DIRENTRY {
    int status;
    int reserved[3];
    char name[20];
    char pad;
};
#endif

/* Error codes (returned by sync functions and callbacks) */
#define McErrNone 0x00         /**< Normal exit / Connected */
#define McErrCardNotExist 0x01 /**< Card is not connected */
#define McErrCardInvalid 0x02  /**< Communication error / Bad card */
#define McErrNewCard 0x03      /**< New card (card was replaced) */
#define McErrNotFormat 0x04    /**< Not formatted */
#define McErrFileNotExist 0x05 /**< File not found */
#define McErrAlreadyExist 0x06 /**< File already exists */
#define McErrBlockFull 0x07    /**< Not enough available blocks */

/* Function command codes (returned in callback/sync cmds parameter) */
#define McFuncExist 0x01     /**< MemCardExist() */
#define McFuncAccept 0x02    /**< MemCardAccept() */
#define McFuncReadFile 0x03  /**< MemCardReadFile() */
#define McFuncWriteFile 0x04 /**< MemCardWriteFile() */
#define McFuncReadData 0x05  /**< MemCardReadData() */
#define McFuncWriteData 0x06 /**< MemCardWriteData() */

/* File open flags */
#define O_RDONLY 0x0001 /**< Read only */
#define O_WRONLY 0x0002 /**< Write only */

/**
 * @brief Memory Card callback function type
 *
 * Callback function signature for asynchronous operations.
 *
 * @param cmds Command code of completed function (McFunc* macros)
 * @param result Result code from the completed function (McErr* macros)
 */
typedef void (*MemCB)(unsigned long cmds, unsigned long result);

/**
 * @brief Check Memory Card status
 *
 * Tests connection with the Memory Card and obtains additional information. If
 * the card is new, _card_clear() and _card_load() are executed, allowing the
 * use of file access functions. MemCardAccept() must be executed before using
 * file access functions. It does not need to be called again as long as the
 * card is not swapped.
 *
 * Asynchronous function (max 76 VSyncs, ~4 VSyncs if no card). Use
 * MemCardSync() or callback to get result.
 *
 * A new card returns McErrNewCard only once. Subsequent calls return
 * McErrNone.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @return 1 if registration successful, 0 otherwise
 */
long MemCardAccept(long chan);

/**
 * @brief Define exit callback
 *
 * Sets the callback function to be triggered when an asynchronous function
 * completes. If func is 0, no callback is generated.
 *
 * @param func Pointer to callback function (MemCB type)
 * @return The address of the previously set callback
 */
MemCB MemCardCallback(MemCB func);

/**
 * @brief Close file
 *
 * Closes the file that was opened with MemCardOpen(). Asynchronous function
 * that exits immediately.
 */
void MemCardClose(void);

/**
 * @brief Create a new file in the Memory Card
 *
 * Creates the specified file in the Memory Card. Synchronous function.
 * Blocking time: 1-4 VSyncs for normal exit, 4-76 VSyncs otherwise.
 * Does not enter critical section.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @param file Filename (ASCII only, no 0x00/0x2a(*)/0x3f(?))
 * @param blocks Number of blocks (1 block = 8192 bytes)
 * @return McErr* code, or -1 if non-synchronous function is active
 */
long MemCardCreateFile(long chan, char* file, long blocks);

/**
 * @brief Delete file from Memory Card
 *
 * Deletes the specified file from the Memory Card. Synchronous function.
 * Blocking time: 1-4 VSyncs for normal exit, 4-76 VSyncs otherwise.
 * Does not enter critical section.
 *
 * Note: Returns McErrCardInvalid if the file is an active PDA application
 * (cannot be deleted due to PDA kernel locking).
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @param file Filename
 * @return McErr* code, or -1 if non-synchronous function is active
 */
long MemCardDeleteFile(long chan, char* file);

/**
 * @brief Terminate Memory Card system
 *
 * Terminates the Memory Card system. Synchronous function.
 * MemCardStop() needs to be executed first if the system was activated from
 * MemCardStart().
 */
void MemCardEnd(void);

/**
 * @brief Get connection status of card
 *
 * Tests the connection status of the Memory Card. Faster than MemCardAccept()
 * since it only checks presence. Use MemCardAccept() for detailed information
 * (formatting, etc.). If cards are swapped, MemCardAccept() must be executed
 * before using file access functions.
 *
 * Asynchronous function (~4 VSyncs). Use MemCardSync() or callback to get
 * result.
 *
 * When a new card is detected (McErrNewCard), call MemCardAccept() to clear
 * the new card status before performing other operations.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @return 1 if command registered successfully, 0 otherwise
 */
long MemCardExist(long chan);

/**
 * @brief Format Memory Card
 *
 * Formats the specified Memory Card. Synchronous function.
 * Blocking time: approximately 144 VSyncs. Does not enter critical section.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @return McErr* code, or -1 if non-synchronous function is active
 */
long MemCardFormat(long chan);

/**
 * @brief Get directory information from Memory Card
 *
 * Finds files matching the filename pattern. Data for matching files are
 * stored in dir, and the total number of matching files is returned in files.
 * Buffer must be prepared by the application.
 *
 * Synchronous function. Blocking time: 1-2 VSyncs for normal exit,
 * 4-76 VSyncs otherwise.
 *
 * Wildcard characters: "?" for any single character, "*" for any number of
 * characters (characters following * are ignored).
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @param name Filename pattern to search (wildcards supported)
 * @param dir Pointer to structure array to hold matching file information
 * @param files Pointer to buffer to receive number of matching files
 * @param ofs Offset for entry (number of files to skip, 0-14)
 * @param max Maximum number of entries to store in buffer
 * @return McErr* code, or -1 if non-synchronous function is active
 */
long MemCardGetDirentry(long chan, char* name, struct DIRENTRY* dir,
                        long* files, long ofs, long max);

/**
 * @brief Initialize Memory Card system
 *
 * Initializes the Memory Card system. If subsequently activated with
 * MemCardStart(), libmcrd functions are available. Should be executed after
 * InitPAD(), InitGUN(), or InitTAP().
 *
 * Set val to 0 when using libtap or libgun.
 *
 * Requires 60-70 VSyncs to complete. Cannot be executed twice.
 *
 * @param val Use of control routine in ROM (0: do not use, 1: use)
 */
void MemCardInit(long val);

/**
 * @brief Open file
 *
 * Opens the specified Memory Card file with the method specified by flag.
 * Once open, MemCardReadData() and MemCardWriteData() can be used.
 *
 * Methods cannot be combined (O_RDONLY|O_WRONLY). Multiple files cannot be
 * opened simultaneously.
 *
 * Synchronous function. Blocking time: Exits immediately for normal
 * completion, 4-76 VSyncs otherwise.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @param file Filename
 * @param flag Open method (O_RDONLY or O_WRONLY)
 * @return McErr* code, or -1 if file already open or non-sync function active
 */
long MemCardOpen(long chan, char* file, long flag);

/**
 * @brief Read data from Memory Card
 *
 * Reads data from the Memory Card file previously opened in MemCardOpen() and
 * stores it in the buffer pointed to by adrs.
 *
 * Asynchronous function. Use MemCardSync() or callback to get result.
 * Time required: ~1 VSync overhead + ~130 VSyncs per block (8192 bytes).
 *
 * bytes must be a multiple of 128. If not, returns 0.
 *
 * @param adrs Pointer to destination buffer in main memory
 * @param offset Offset in bytes from file start (0-based)
 * @param bytes Number of bytes to read (must be multiple of 128)
 * @return 1 if registered successfully, 0 otherwise
 */
long MemCardReadData(unsigned long* adrs, long offset, long bytes);

/**
 * @brief Read file from Memory Card
 *
 * Reads data from the specified Memory Card file and stores it in the buffer
 * pointed to by adrs. Executes MemCardOpen() and MemCardReadData() internally.
 *
 * Asynchronous function. Use MemCardSync() or callback to get result.
 * Time required: ~1 VSync overhead + ~130 VSyncs per block (8192 bytes).
 *
 * bytes must be a multiple of 128. If not, returns 0.
 *
 * If file is already open, returns 0.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @param file Filename
 * @param adrs Pointer to destination buffer in main memory
 * @param offset Offset in bytes from file start (0-based)
 * @param bytes Number of bytes to read (must be multiple of 128)
 * @return 1 if registered successfully, 0 if file open or async active
 */
long MemCardReadFile(
    long chan, char* file, unsigned long* adrs, long offset, long bytes);

/**
 * @brief Start Memory Card system
 *
 * Places the Memory Card system, previously initialized with MemCardInit(), in
 * an active state. Internally, eight events such as HwCARD and SwCARD are
 * opened.
 *
 * Asynchronous function. Exits immediately.
 */
void MemCardStart(void);

/**
 * @brief Stop Memory Card system
 *
 * Stops the Memory Card system activated by MemCardStart(). Various events are
 * closed.
 *
 * Asynchronous function. Exits immediately.
 */
void MemCardStop(void);

/**
 * @brief Wait for completion of async function or check status
 *
 * If mode is 0, waits for termination of an asynchronous function.
 * If mode is 1, exits immediately and returns the status.
 *
 * cmds stores the operation code of the terminated function (McFunc* macros).
 * result stores the execution result (McErr* macros).
 *
 * @param mode 0: wait for termination, 1: check status and return immediately
 * @param cmds Pointer to receive terminated function code
 * @param result Pointer to receive execution result
 * @return 0: still active, 1: terminated, -1: no registered process
 */
long MemCardSync(long mode, long* cmds, long* result);

/**
 * @brief Uninitialize a Memory Card (debugging only)
 *
 * Puts Memory Card in uninitialized (unformatted) state. Synchronous function.
 *
 * This is a debugging function used to create an unformatted card for testing.
 * Should NOT be used in actual titles.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @return 1: completed successfully, 0: error, -1: async function running
 */
long MemCardUnformat(long chan);

/**
 * @brief Write data to Memory Card
 *
 * Writes data from the buffer pointed to by adrs to the Memory Card file
 * previously opened with MemCardOpen().
 *
 * Asynchronous function. Use MemCardSync() or callback to get result.
 * Time required: ~1 VSync overhead + ~130 VSyncs per block (8192 bytes).
 *
 * bytes must be a multiple of 128. If not, returns 0.
 *
 * @param adrs Pointer to source buffer in main memory
 * @param offset Offset in bytes from file start (0-based)
 * @param bytes Number of bytes to write (must be multiple of 128)
 * @return 1 if registered successfully, 0 otherwise
 */
long MemCardWriteData(unsigned long* adrs, long offset, long bytes);

/**
 * @brief Write file to Memory Card
 *
 * Writes data from the buffer pointed to by adrs to the specified Memory Card
 * file. If the file is new, it must be created beforehand with
 * MemCardCreateFile(). Executes MemCardOpen() and MemCardWriteData()
 * internally.
 *
 * Asynchronous function. Use MemCardSync() or callback to get result.
 * Time required: ~1 VSync overhead + ~130 VSyncs per block (8192 bytes).
 *
 * bytes must be a multiple of 128. If not, returns 0.
 *
 * If file is already open, returns 0.
 *
 * @param chan Port number + card number (Port A: 0x00, Port B: 0x10)
 * @param file Filename
 * @param adrs Pointer to source buffer in main memory
 * @param offset Offset in bytes from file start (0-based)
 * @param bytes Number of bytes to write (must be multiple of 128)
 * @return 1 if registered successfully, 0 if file open or async active
 */
long MemCardWriteFile(
    long chan, char* file, unsigned long* adrs, long offset, long bytes);

#endif /* LIBMCRD_H */
