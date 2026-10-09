#ifndef LIBAPI_H
#define LIBAPI_H

#include <psyz.h>
#include <kernel.h>
#include <romio.h>

/* lseek flags */
#define SEEK_SET 0 /**< Seek from start of file */
#define SEEK_CUR 1 /**< Seek from current position */

/* Memory Card functions */

/**
 * @brief Initialize Memory Card BIOS
 *
 * Initializes the Memory Card BIOS and enters an idle state. val specifies
 * whether or not there is sharing with the controller. (0: not shared; 1:
 * shared.) When the BIOS is subsequently put into operation by StartCARD(), the
 * low-level interface functions that begin with "_card" can be used directly.
 * The Memory Card file system uses these interfaces internally, so InitCARD()
 * needs to be executed before _bu_init(). There is no effect on the controller.
 *
 * @param val Specify sharing with controller (0: not shared; 1: shared)
 */
void InitCARD(long val);

/**
 * @brief Start Memory Card BIOS
 *
 * Changes the Memory Card BIOS initialized by InitCARD() to a run state.
 * Performs ChangeClearPAD(1) internally.
 */
long StartCARD(void);

/**
 * @brief Stop Memory Card BIOS
 *
 * Changes Memory Card BIOS to an idle state--the same state as that immediately
 * after executing InitCARD(). It also stops the controller. It is necessary to
 * call StartPAD() to start the controller.
 */
long StopCARD(void);

/**
 * @brief Initialize Memory Card file system
 *
 * Initializes the Memory Card file system. This file system is not initialized
 * automatically, so it is necessary to call this function.
 *
 * This function must be called after InitCARD() and StartCARD().
 */
void _bu_init(void);

/**
 * @brief Get card status
 *
 * Tests the connection of the Memory Card specified in chan.
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously, so it returns immediately. Processing
 * completion is communicated by an event (SwCARD/EvSpIOE, SwCARD/EvSpTIMOUT,
 * SwCARD/EvSpNEW, or SwCARD/EvSpERROR).
 *
 * Do not use _new_card() to suppress EvSpNEW.
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if registration successful, otherwise 0
 */
long _card_info(long chan);

/**
 * @brief Clear unconfirmed flags
 *
 * Performs a dummy write to the system management area of the card and clears
 * the card's unconfirmed flags.
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously, so it returns immediately. Processing
 * completion is communicated by an event (HwCARD/EvSpIOE, HwCARD/EvSpTIMOUT,
 * HwCARD/EvSpNEW, HwCARD/EvSpERROR, or HwCARD/EvSpUNKOWN).
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if registration successful, otherwise 0
 */
long _card_clear(long chan);

/**
 * @brief Test logical format
 *
 * Reads file management information for the card specified by chan in the
 * file system in order to get asynchronous access using the I/O management
 * service.
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * _card_load() must be called at least once before you can use open() on a
 * Memory Card file in O_NOWAIT mode. It does not have to be called again
 * unless a card is changed.
 *
 * This function executes asynchronously, so it returns immediately. Processing
 * completion is communicated by an event (SwCARD/EvSpIOE, SwCARD/EvSpTIMOUT,
 * SwCARD/EvSpNEW, or SwCARD/EvSpERROR).
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if the read is successful, otherwise 0
 */
long _card_load(long chan);

/**
 * @brief Set automatic format function
 *
 * When val is 0, the automatic format function is disabled; when val is 1, it
 * is enabled. This function should be used for testing purposes only.
 *
 * @param val Indicates automatic formatting
 * @return Previously set automatic format value
 */
long _card_auto(long val);

/**
 * @brief Change settings of unconfirmed flag test
 *
 * Masks the generation of an EvSpNEW event immediately after _card_read() or
 * _card_write().
 *
 * Terminates immediately even though it is a synchronous function.
 */
void _new_card(void);

/**
 * @brief Get Memory Card BIOS status
 *
 * Gets the Memory Card BIOS status of each slot, drv. Specify drv as 0 for
 * Port 1, 1 for Port 2.
 *
 * This is a synchronous function.
 *
 * If the Memory Card BIOS is in run state, it can return any of the following
 * values:
 * - 0x01: Idle processing
 * - 0x02: READ processing
 * - 0x04: WRITE processing
 * - 0x08: Connection test processing registration
 * - 0x11: No registered processing (just prior to EvSpTIMOUT generation)
 * - 0x21: No registered processing (just prior to EvSpERROR generation)
 *
 * @param drv Port number (0: Port 1, 1: Port 2)
 * @return Memory Card BIOS status value
 */
long _card_status(long drv);

/**
 * @brief Wait for Memory Card BIOS completion
 *
 * Wait until registration processing completes for the drv slot. Specify drv
 * as 0 for Port 1, 1 for Port 2.
 *
 * @param drv Sets slot number (0: Port 1, 1: Port 2)
 * @return Always 1
 */
long _card_wait(long drv);

/**
 * @brief Get a Memory Card BIOS event
 *
 * Returns the device number of the Memory Card that just generated an event.
 *
 * @return 2-digit hex device number
 */
unsigned long _card_chan(void);

/**
 * @brief Write to one block of the Memory Card (for testing only)
 *
 * Writes 128 bytes of buffer data pointed to by buf to the target block number
 * (block) of the Memory Card of the specified channel (chan).
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously, so it returns immediately. Actual
 * processing termination is communicated by an event (HwCARD/EvSpIOE,
 * HwCARD/EvSpTIMOUT, HwCARD/EvSpNEW, HwCARD/EvSpERROR, or HwCARD/EvSpUNKOWN).
 * Multiplex processing to the same card slot can't be performed.
 *
 * This is a low-level function that should be used for testing only. It
 * bypasses the memory card file system; therefore, in a released product,
 * use the C file-handling routines such as write().
 *
 * @param chan Port number x 16 + card number
 * @param block Target block number
 * @param buf Pointer to 128-byte data buffer
 * @return 1 if registration successful, otherwise 0
 */
long _card_write(long chan, long block, unsigned char* buf);

/**
 * @brief Read one block from the Memory Card
 *
 * Reads 128 bytes of buffer data into buf from the target block number (block)
 * of the Memory Card of the specified channel (chan).
 *
 * When calculating chan, "port number" is 0 for Port 1 and 1 for Port 2.
 * "Card number" is zero when a standard controller is connected, and may be
 * in the range 0-3 if a Multi Tap is connected.
 *
 * This function executes asynchronously so it returns immediately after
 * completion. Actual processing termination is communicated by an event
 * (HwCARD/EvSpIOE, HwCARD/EvSpTIMOUT, HwCARD/EvSpNEW, HwCARD/EvSpERROR, or
 * HwCARD/EvSpUNKOWN). Multiplex processing to the same card slot can't be
 * performed.
 *
 * This function exists within the low-level interface and is one of the
 * special functions used for testing.
 *
 * @param chan Port number x 16 + card number
 * @param block Target block number
 * @param buf Pointer to 128 byte data buffer
 * @return 1 if successful processing registration, otherwise 0
 */
long _card_read(long chan, long block, unsigned char* buf);

/**
 * @brief Format the Memory Card
 *
 * Formats the Memory Card. When calculating chan, "port number" is 0 for
 * Port 1 and 1 for Port 2. "Card number" is zero when a standard controller
 * is connected, and may be in the range 0-3 if a Multi Tap is connected.
 *
 * Does not enter critical section. Synchronous functions are blocked for
 * approximately 144 Vsync.
 *
 * @param chan Port number x 16 + Card number
 * @return 1 if formatting is successful, otherwise 0
 */
long _card_format(long chan);

/* Memory allocation functions */

/**
 * @brief Initialize heap area
 *
 * Initializes a group of standard function library memory control functions.
 * After using this function, malloc(), free(), etc. are usable.
 * There is some overhead, so the entire size in bytes cannot be used.
 * Must be executed in a critical section. If several executions of this
 * function overlap, the previous memory control information is lost.
 *
 * @param head Pointer to heap start address
 * @param size Heap size (a multiple of 4, in bytes)
 */
void InitHeap(unsigned long* head, unsigned long size);

/* File I/O functions */
#ifndef __psyz
/**
 * @brief Open a file
 *
 * Opens a device for low-level input/output, and returns the descriptor.
 *
 * @param devname Pointer to a filename
 * @param flag Open mode (O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_NOBUF,
 * O_NOWAIT)
 * @return The file descriptor, if the function succeeds. On failure, it returns
 * -1.
 */
int open(const char* devname, int flag);

/**
 * @brief Close a file
 *
 * Closes the file specified by fd.
 *
 * @param fd File descriptor
 * @return fd, if the function succeeds, -1 otherwise.
 */
int close(int fd);

/**
 * @brief Read data from a file
 *
 * Reads n bytes from the descriptor fd to the area specified by buf.
 *
 * @param fd File descriptor
 * @param buf Pointer to read buffer address
 * @param n Number of bytes to read
 * @return The actual number of bytes read into the area. An error returns -1.
 */
long read(long fd, void* buf, long n);

/**
 * @brief Write data to a file
 *
 * Writes n bytes from the descriptor fd to the area specified by buf.
 *
 * @param fd File descriptor
 * @param buf Pointer to the write buffer address
 * @param n Number of bytes to be written
 * @return The number of bytes actually written to the area. If there is an
 * error, -1 is returned.
 */
int write(int fd, char* buf, int n);

/**
 * @brief Move a file pointer
 *
 * Moves a file pointer of the device indicated by the descriptor fd.
 * If flag is SEEK_SET, movement starts at the start of the file;
 * if flag is SEEK_CUR, movement starts with the current position.
 * This function does not apply to a PC communication link.
 *
 * @param fd File descriptor
 * @param offset Number of bytes to move pointer
 * @param flag Start point flag (SEEK_SET or SEEK_CUR)
 * @return The current file pointer, if it succeeds. On failure, it returns -1.
 */
unsigned long lseek(int fd, unsigned int offset, int flag);

/**
 * @brief Control devices
 *
 * Executes control commands on the device. Details of the commands and their
 * arguments are given separately for each device.
 *
 * @param fd File descriptor
 * @param com Control command
 * @param arg Control command argument
 * @return 1 if it succeeds and 0 otherwise.
 */
long ioctl(int fd, int com, int arg);
#endif

/* File system functions */

/**
 * @brief Initialize file system
 *
 * Initializes file system fs. This function is only effective on writeable file
 * systems. When initializing the Memory Card, it's preferable to use the
 * libcard function _card_format().
 *
 * @param fs Pointer to file system name
 * @return Always returns 1.
 */
long format(char* fs);

/**
 * @brief Change default directory
 *
 * Changes the default directory path for a given file system (specified by the
 * device name at the beginning of path).
 *
 * @param path Pointer to the default directory path
 * @return 1 if it succeeds, and 0 otherwise.
 */
long cd(char* path);

/**
 * @brief Find the first file matching a filename
 *
 * Finds the first file corresponding to the filename pattern name, and stores
 * data relating to this file in the directory dir. The wildcard characters "?"
 * (standing for any one character) and "*" (standing for a character string of
 * any length) can be used in the filename pattern. Characters specified after
 * "*" are ignored.
 *
 * @param name Pointer to a filename
 * @param dir Pointer to the buffer holding information relating to the
 * referenced file
 * @return Returns dir if it succeeds, and 0 otherwise.
 */
struct DIRENTRY* firstfile(char* name, struct DIRENTRY* dir);

/**
 * @brief Find the next file matching a filename
 *
 * Continues a file lookup under the same conditions as the previous call to
 * firstfile(). If it finds a corresponding file, it stores file information in
 * dir. If the shell cover of the CD-ROM drive has been opened since the
 * execution of the previous firstfile() call, this function fails, and reports
 * that the file has not been found.
 *
 * @param dir Pointer to buffer holding file information
 * @return dir if it succeeds, and 0 otherwise.
 */
struct DIRENTRY* nextfile(struct DIRENTRY* dir);

/**
 * @brief Delete a file
 *
 * Deletes the file specified by name.
 * This function was formerly called "delete."
 *
 * @param name Pointer to a filename
 * @return 1 on success, 0 on failure.
 */
long erase(char* name);

/**
 * @brief Resurrect a file
 *
 * Resurrects the previously deleted file specified by name.
 *
 * @param name Pointer to filename
 * @return 1 if it succeeds, and 0 otherwise.
 */
long undelete(char* name);

#ifndef __psyz
/**
 * @brief Change a file name
 *
 * Changes a filename from src to dest. In both cases, the full path from the
 * device name must be specified. This function is only effective on writeable
 * file systems.
 *
 * @param src Pointer to the old filename
 * @param dest Pointer to the new filename
 * @return 1 if it succeeds, and 0 otherwise.
 */
long rename(char* src, char* dest);
#endif

/* Program execution functions */

/**
 * @brief Load an execution file
 *
 * Reads the PlayStation EXE format file name to the address specified by its
 * internal header, and writes internal information to exec. This function needs
 * the ISO 9660 file system to run properly. To initialize this system, call
 * _96_init(); to exit the system, call _96_remove(). Calls FlushCache()
 * internally.
 *
 * @param name Pointer to filename
 * @param exec Pointer to execution file information
 * @return 1 if it succeeds, and 0 otherwise.
 */
long Load(char* name, struct EXEC* exec);

/**
 * @brief Execute an execution file
 *
 * Executes a module that has already been loaded into memory, using the
 * execution file information specified by exec. If exec->s_addr is 0, neither
 * the stack nor frame pointers are set. It must be executed in a critical
 * section. This function needs the ISO 9660 file system to run properly. Call
 * _96_init() to initialize the system and _96_remove() to exit the system.
 *
 * @param exec Pointer to execution file information
 * @param argc Number of arguments
 * @param argv Pointer to argument
 * @return 1 on success; 0 on failure.
 */
long Exec(struct EXEC* exec, long argc, char* argv);

/**
 * @brief Load and execute an execution file
 *
 * Calls Load() and Exec(), then reads a file name into memory and executes the
 * file. s_addr and s_size are passed to Exec() and set by the structure EXEC.
 * This function needs the ISO 9660 file system to run properly. To initialize
 * this system, call _96_init(); to exit the system, call _96_remove().
 *
 * @param name Pointer to a PS-X EXE format execution file name (fewer than 19
 * characters)
 * @param s_addr Stack area starting address
 * @param s_size Number of bytes in stack area
 */
void LoadExec(char* name, unsigned long s_addr, unsigned long s_size);

/**
 * @brief Load test
 *
 * Writes internal information from a PS-X EXE format file name to exec.
 *
 * @param name Pointer to filename
 * @param exec Pointer to data in an execution file
 * @return The execution starting address. On failure, it returns 0.
 */
long LoadTest(char* name, struct EXEC* exec);

/* Controller/PAD functions */

/**
 * @brief Initialize the controller
 *
 * Registers a receive data buffer for the controller.
 * Since it is possible for mistakes to occur when an unexpected controller is
 * connected to the receive data length, always allocate 34 bytes. When using
 * the Multi Tap, use InitTAP(). When using the gun controller, use InitGUN().
 *
 * @param bufA Pointer to incoming data buffer for port A
 * @param lenA Length of incoming data buffer for port A (in bytes)
 * @param bufB Pointer to incoming data buffer for port B
 * @param lenB Length of incoming data buffer for port B (in bytes)
 * @return Always 1.
 */
int InitPAD(char* bufA, int lenA, char* bufB, int lenB);

/**
 * @brief Start reading the controller
 *
 * Triggered by the interruption of a vertical retrace line, this function
 * starts to read the controller. ChangeClearPAD(1) is executed internally.
 * Interrupts are permitted.
 *
 * @return Always returns 1.
 */
long StartPAD(void);

/**
 * @brief Stop reading the controller
 *
 * Stops reading the controller. Interrupts are not permitted.
 */
void StopPAD(void);

/**
 * @brief Low-level PAD driver init (BIOS A-vector 0x13).
 *
 * Initializes the low-level controller driver state. Games sometimes invoke
 * this directly via the BIOS A-vector; high-level code should prefer
 * PadInit() or InitPAD().
 */
int PAD_init(int type, void* unused);

/**
 * @brief Low-level PAD read (BIOS A-vector 0x14).
 *
 * Samples one controller port into a caller-provided buffer using the
 * canonical PSX SIO frame layout. Returns the number of bytes written.
 *
 * @param port Controller port (0 or 1)
 * @param dst Caller buffer, expected to be at least 34 bytes
 * @return Bytes written (34 on success, 0 on failure)
 */
int PAD_dr(int port, char* dst);

/**
 * @brief Set the control driver
 *
 * if val is 1, interrupt processing in a control driver started by a vertical
 * retrace line interrupt is completed. If val is 0, processing is passed to a
 * lower priority interrupt module without completion.
 *
 * @param val Vertical retrace line interruption clear flag
 */
void ChangeClearPAD(long val);

/**
 * @brief Enable communication with the controller
 *
 * Enables communication with a controller which was disabled with DisablePAD().
 * Although a normal controller communicates via Vsync interrupts, this function
 * is used only with timing longer than 1/60 sec. when the controller status is
 * not needed.
 */
void EnablePAD(void);

/**
 * @brief Disable communication with the controller
 *
 * Temporarily disables communication with the controller.
 * Unlike StopPAD(), which deletes the controller handler activated by Vsync
 * interrupts, this function simply skips controller communication by setting a
 * flag in the handler. Since a controller normally communicates via Vsync
 * interrupts, this function can be used in situations when the controller
 * status is needed less frequently than every 1/60 sec.
 */
void DisablePAD(void);

/* Event functions */

/**
 * @brief Open an event
 *
 * Secures an EvCB for an event with the descriptor desc and event class spec.
 * Must be executed in a critical section.
 *
 * @param desc Cause descriptor
 * @param spec Event type
 * @param mode Mode
 * @param func Pointer to the handler function
 * @return The event descriptor, if the function succeeds. On failure, it
 * returns -1.
 */
long OpenEvent(unsigned long desc, long spec, long mode, long (*func)());

/**
 * @brief Close an event
 *
 * Releases the EvCB specified by event. Must be executed in a critical section.
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure.
 */
long CloseEvent(long event);

/**
 * @brief Generate an event
 *
 * Delivers an event if the event's current status is EvStACTIVE (event not yet
 * generated, generation possible). If the event mode is EvMdINTR, the event
 * handler function is called. If the event mode is EvMdNOINTR, the event status
 * is changed to EvStALREADY (event already occurred, generation prohibited).
 * This function must be executed in a critical section.
 *
 * @param ev1 Cause descriptor
 * @param ev2 Event class
 */
void DeliverEvent(unsigned ev1, unsigned ev2);

/**
 * @brief Cancel an event
 *
 * Returns event state from EvStALREADY (already occurred) to EvStACTIVE if the
 * event mode is EvMdNOINTR. Must be executed in a critical section.
 *
 * @param ev1 Cause descriptor
 * @param ev2 Event class
 */
void UnDeliverEvent(unsigned ev1, unsigned ev2);

/**
 * @brief Test an event
 *
 * Checks to see whether or not the event specified by the descriptor event has
 * occurred. If so, the function restores the event state to EvStACTIVE.
 *
 * @param event Event descriptor
 * @return 1 if the event is found to have occurred, 0 otherwise.
 */
long TestEvent(long event);

/**
 * @brief Wait for the occurrence of an event
 *
 * Waits until an event specified by the descriptor event occurs, and returns
 * after restoring the event state to EvStACTIVE.
 *
 * @param event Event descriptor
 * @return 1 if it succeeds, and 0 otherwise.
 */
long WaitEvent(long event);

/**
 * @brief Enable occurrence of an event
 *
 * Enables occurrence of an event specified by the descriptor event. It changes
 * the event status to EvStACTIVE (event not yet generated, generation
 * possible).
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure.
 */
long EnableEvent(long event);

/**
 * @brief Disable an event
 *
 * Inhibits occurrence of an event specified by the descriptor event. It changes
 * the event status to EvStWAIT (event generation prohibited).
 *
 * @param event Event descriptor
 * @return 1 on success, 0 on failure.
 */
long DisableEvent(long event);

/* Thread functions */

/**
 * @brief Open a thread
 *
 * Secures a TCB for a given thread, and initializes it with the arguments
 * given. Must be executed in a critical section. The thread can be executed
 * using ChangeTh().
 *
 * @param func Pointer to the execution start function
 * @param sp Stack pointer value
 * @param gp Global pointer value
 * @return The thread descriptor, if the function succeeds. On failure, it
 * returns -1.
 */
unsigned long OpenTh(
    unsigned long (*func)(), unsigned long sp, unsigned long gp);

/**
 * @brief Close a thread
 *
 * Closes a thread and releases its TCB. Must be executed in a critical section.
 *
 * @param thread Thread descriptor
 * @return 1 on success, 0 on failure.
 */
long closeTh(unsigned long thread);

/**
 * @brief Change the thread to be executed
 *
 * Transfers execution to the thread specified by thread. The current thread is
 * saved in a TCB. This function returns when the original thread is restored.
 * Before executing ChangeTh(), initialize TCB reg[R-SR] to the following:
 * - The interrupt context is 0X404
 * - The main flow is 0X401
 *
 * @param thread Thread descriptor
 * @return On success and re-execution, the function returns 1. On failure, it
 * returns 0. The return value on reexecution can be changed by any other
 * thread.
 */
long ChangeTh(unsigned long thread);

/* Root counter functions */

/**
 * @brief Set a root counter
 *
 * Sets the root counter in spec to the target value in target, and the mode in
 * mode.
 *
 * @param spec Root counter specification
 * @param target Target value
 * @param mode Mode
 * @return 1 if it succeeds, and 0 otherwise. (0 is always returned if you
 * specify RCntCNT3, since it can't be set.)
 */
long SetRCnt(long spec, unsigned short target, long mode);

/**
 * @brief Get value of a root counter
 *
 * Returns the current value of root counter spec. To be used when root counter
 * spec has been set by SetRCnt to a polling mode (RCntMdNOINTR).
 *
 * @param spec Root counter
 * @return The 32-bit unsigned expanded counter value. On failure, it returns
 * -1.
 */
long GetRCnt(long spec);

/**
 * @brief Start a root counter
 *
 * Enables interrupts for root counter spec.
 *
 * @param spec Root counter
 * @return 1 if it succeeds, and 0 otherwise. (0 is always returned if you
 * specify RCntCNT3, since it can't be set.)
 */
long StartRCnt(long spec);

/**
 * @brief Stop a root counter
 *
 * Disables interrupts for root counter spec.
 *
 * @param spec Root counter
 * @return 1 if it succeeds, and 0 otherwise. (0 is always returned if you
 * specify RCntCNT3, since it can't be set.)
 */
long StopRCnt(long spec);

/**
 * @brief Reset a root counter
 *
 * Resets the root counter spec to 0.
 *
 * @param spec Root counter specification
 * @return 1 if it succeeds, and 0 otherwise. (0 is always returned if you
 * specify RCntCNT3, since it can't be set.)
 */
long ResetRCnt(long spec);

/* Kernel configuration functions */

/**
 * @brief Modify the kernel configuration
 *
 * Modifies system configuration parameters. The contents of event and task
 * control blocks, and all settings for event handlers and callback functions in
 * each library, are destroyed. However, file descriptors are not affected (all
 * the descriptors should be closed before SetConf() call) because most of the
 * device drivers are driven by the event handler. All patches to the kernel are
 * invalidated. This function should be executed at the head of the first
 * execution file.
 *
 * @param ev Number of event control block (EvCB) elements
 * @param tcb Number of task control block (TCB) elements
 * @param sp Ignored
 * @return 1 if the function succeeds, 0 otherwise.
 */
long SetConf(unsigned long ev, unsigned long tcb, unsigned long sp);

/**
 * @brief Get the kernel configuration
 *
 * Stores a system configuration parameter set by SetConf() to the address given
 * by the pointer as the argument. It returns an undefined value before the
 * execution of SetConf() because this function refers to its internal
 * parameter.
 *
 * @param ev Pointer to number of event management blocks
 * @param tcb Pointer to number of task management blocks
 * @param sp Ignored
 */
void GetConf(unsigned long* ev, unsigned long* tcb, unsigned long* sp);

/**
 * @brief Modify the valid memory size
 *
 * Changes the valid memory size to n. It must be 2 or 8 (megabytes); any other
 * values are ignored. Memory access out of the valid range causes a CPU
 * exception regardless of how much physical memory is present.
 *
 * @param n Valid memory size (in megabytes)
 */
void SetMem(unsigned long n);

/* Register access functions */

/**
 * @brief Get value of gp register
 *
 * Gets the value of the gp register.
 *
 * @return The current gp register value.
 */
unsigned long GetGp(void);

/**
 * @brief Get value of stack pointer
 *
 * Gets value of sp register.
 *
 * @return The current sp register value.
 */
unsigned long GetSp(void);

/**
 * @brief Set the stack pointer
 *
 * Sets the sp register to the value new_sp.
 *
 * @param new_sp value to set sp register
 * @return The previous sp register value.
 */
unsigned long SetSp(unsigned long new_sp);

/**
 * @brief Get value of status register
 *
 * Gets the value of the status register.
 *
 * @return The current status register value.
 */
unsigned long GetSr(void);

/**
 * @brief Get cause register value
 *
 * Gets the value of the cause register (a coprocessor control register).
 *
 * @return The current cause register value.
 */
unsigned long GetCr(void);

/**
 * @brief Get address of system stack
 *
 * Gets the highest address of a system stack area for event handler function
 * execution. The size of the stack area is 2 K-bytes.
 *
 * @return Highest address of the system stack area
 */
long GetSysSp(void);

/* Cache and exception functions */

/**
 * @brief Flush instruction cache
 *
 * Flushes the instruction cache (I-cache). Must be executed in a critical
 * section. Because this function can hang if it is called during DMA transfer,
 * it must be called after confirming that DMA transfer is complete.
 */
void FlushCache(void);

/**
 * @brief Cause an interrupt
 *
 * Causes an interrupt, and stores the current context in the execution TCB.
 * It is also valid in a critical section. Executes an internal call and
 * destroys the exception context.
 */
void Exception(void);

/**
 * @brief Return from exception
 *
 * Accesses the exception context and returns from exception processing.
 * It is used in an event handler or callback function.
 */
void ReturnFromException(void);

/**
 * @brief Suppress interrupts
 *
 * Suppresses interrupts. Because no system call interrupt is generated
 * internally, this function can be invoked in event handling and callback
 * functions. It must be executed in a critical section.
 */
void SwEnterCriticalSection(void);

/**
 * @brief Enable interrupts
 *
 * Enables interrupts. Because no system call interrupt is generated internally,
 * this function can be invoked in event handling and callback functions. Must
 * be executed in a critical section.
 */
void SwExitCriticalSection(void);

/* Font functions */

/**
 * @brief Get Kanji font pattern addresses
 *
 * Gets the starting address in the kernel of the font pattern for the Kanji
 * character specified by sjiscode. Bug alert: The normal arguments are
 * Shift-JIS code values between 0x8140~0x84BE or 0x889F~0x9872. If a Shift-JIS
 * code within that region corresponds to a blank area in the code table, a font
 * pattern unrelated to that code is returned as the starting address. This
 * problem has been corrected in Krom2RawAdd2, so we recommend using
 * Krom2RawAdd2 to obtain the font pattern starting address.
 *
 * @param sjiscode Shift-JIS code
 * @return The starting address of a Kanji font pattern. If there is no font
 * data corresponding to the specified Kanji character, a value of -1 is
 * returned.
 */
unsigned long Krom2RawAdd(unsigned short sjiscode);

/**
 * @brief Get shift-JIS font pattern addresses
 *
 * Gets the starting address in the font pattern kernel for the non-Kanji/Kanji
 * level 1 character specified by the sjiscode.
 *
 * @param sjiscode Shift-JIS code
 * @return The font pattern starting address. When there is no font data
 * corresponding to the specified shift-JIS code, an address containing a full
 * space font pattern is returned.
 */
unsigned long Krom2RawAdd2(unsigned short sjiscode);

/* System functions */

/**
 * @brief Display the system error screen
 *
 * Displays a detected system error for the user. On the PlayStation, it calls
 * exit().
 *
 * @param c Error identification character (Alphabetic character)
 * @param n Error identification code (0 to 999)
 */
void SystemError(char c, long n);

/**
 * @brief Install the ISO-9660 file system
 *
 * Installs the ISO-9660 file system driver that manages access to the CD-ROM.
 */
void _96_init(void);

/**
 * @brief Remove the ISO-9660 file system
 *
 * Removes the ISO-9660 file system driver that manages access to the CD-ROM.
 */
void _96_remove(void);

/**
 * @brief Reboot the system
 *
 * Reboots the system. This function is useful for demonstration programs; don't
 * use it for general title applications.
 */
void _boot(void);

/**
 * @brief Get the latest I/O error code
 *
 * Gets the latest error code from all file descriptors. (Error codes are
 * defined in sys/errno.h.)
 *
 * @return Error code.
 */
int _get_errno(void);

/**
 * @brief Get an error code for a file descriptor
 *
 * Gets the most recent error code of the specified file descriptor.
 * (Error codes are defined in sys/errno.h.)
 *
 * @param fd File descriptor
 * @return Error code.
 */
int _get_error(int fd);

#endif
