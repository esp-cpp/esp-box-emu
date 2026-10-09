#ifndef LIBMCX_H
#define LIBMCX_H
#include <psyz/types.h>

/**
 * @file libmcx.h
 * @brief PDA (PocketStation) Library
 *
 * This library provides functions for communication with the PocketStation
 * (PDA) peripheral. It supports application execution, memory access,
 * real-time clock operations, and various PDA-specific features.
 *
 * All McxXXX() functions (except McxStartCom, McxStopCom, McxSync) perform
 * process registration only. Use McxSync() to check for process completion
 * before accessing result buffers or calling another process registration
 * function.
 *
 * Port numbers:
 * - Port 1: 0x00-0x03 (direct connect and multitap A-D)
 * - Port 2: 0x10-0x13 (direct connect and multitap A-D)
 */

/* Error codes (McxSync result values) */
#define McxErrSuccess 0 /**< Normal termination */
#define McxErrNoCard 1  /**< Neither PDA nor Memory Card inserted */
#define McxErrInvalid 2 /**< Communication failure */
#define McxErrNewCard 3 /**< Normal termination (card has been swapped) */

/* McxSync return values */
#define McxSyncRun 0   /**< Processing */
#define McxSyncFin 1   /**< Process complete */
#define McxSyncNone -1 /**< Process unregistered */

/* Command number macros (returned in McxSync cmd parameter) */
#define McxFuncGetApl 1     /**< McxGetApl() */
#define McxFuncExecApl 2    /**< McxExecApl() */
#define McxFuncGetTime 3    /**< McxGetTime() */
#define McxFuncGetMem 4     /**< McxGetMem() */
#define McxFuncSetMem 5     /**< McxSetMem() */
#define McxFuncShowTrans 6  /**< McxShowTrans() */
#define McxFuncHideTrans 7  /**< McxHideTrans() */
#define McxFuncCurrCtrl 8   /**< McxCurrCtrl() */
#define McxFuncSetLED 9     /**< McxSetLED() */
#define McxFuncGetSerial 10 /**< McxGetSerial() */
#define McxFuncExecFlag 11  /**< McxExecFlag() */
#define McxFuncAllInfo 12   /**< McxAllInfo() */
#define McxFuncFlashAcs 13  /**< McxFlashAcs() */
#define McxFuncReadDev 14   /**< McxReadDev() */
#define McxFuncWriteDev 15  /**< McxWriteDev() */
#define McxFuncGetUIFS 16   /**< McxGetUIFS() */
#define McxFuncSetUIFS 17   /**< McxSetUIFS() */
#define McxFuncSetTime 18   /**< McxSetTime() */
#define McxFuncCardType 19  /**< McxCardType() */

/**
 * @brief Get all PDA information (process registration)
 *
 * With 1 Vsync this function can simultaneously obtain the block number of the
 * header which contains an executing PDA application, the PDA application's
 * flash memory priority write settings, the state of the PDA's current capacity
 * controls, the serial number, and time and date information. This function
 * performs process registration only. Use McxSync() to check for process
 * completion.
 *
 * Buffer contents (18 bytes):
 * - Offset 0-1: Executing PDA application number (LSB, MSB)
 * - Offset 2: Speaker disabled
 * - Offset 3: IR communication disabled
 * - Offset 4-7: PDA serial number (LSB to MSB)
 * - Offset 8-15: Real-time clock (100 years, year, month, day, day of week,
 *                hours, minutes, seconds)
 * - Offset 16: PDA application's flash write priority disabled
 * - Offset 17: LED disabled
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param state Pointer to buffer for storing results (must be 18 bytes)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxAllInfo(int port, unsigned char* state);

/**
 * @brief Probe PDA connection status (process registration)
 *
 * This function is used to check for the presence of a PDA. The value of the
 * result returned from McxSync() is used to make the determination. Once card
 * connection is confirmed using a function such as MemCardAccept() or
 * _card_info(), McxCardType() can be called. McxErrSuccess means PDA detected,
 * McxErrInvalid means Memory Card detected.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxCardType(int port);

/**
 * @brief Control current capacity (process registration)
 *
 * Limits the speaker, IR transmission and LED among PDA functions. This is
 * necessary because there is an upper limit to the current that the PlayStation
 * can supply to the front terminals (160mA total for both ports). Immediately
 * after the PDA is inserted into the PlayStation, all become disabled by
 * default.
 *
 * Current consumption:
 * - CPU chip: 10mA
 * - IR module transmission: 70mA
 * - Speaker: 20mA
 * - LED: 10mA
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param sound 1: Disable speaker, 0: Enable speaker
 * @param infred 1: Disable IR transmit, 0: Enable IR transmit
 * @param led 1: Disable LED, 0: Enable LED
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxCurrCtrl(int port, int sound, int infred, int led);

/**
 * @brief Execute a PDA application (process registration)
 *
 * The PDA application with the specified header block number is executed. If
 * the specified block number is not a header block of a PDA application, proper
 * PDA operation cannot be guaranteed. Use McxGetApl() or McxAllInfo() after
 * calling this function to confirm that the target application has started.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param aplno Block number of header which contains the desired PDA
 *              application (0-15)
 * @param arg Argument passed to the application to be started
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxExecApl(int port, int aplno, long arg);

/**
 * @brief Set the PDA application / data ID flag (process registration)
 *
 * Called and set when downloading a PDA application to the PDA. The PDA
 * application flag is set to 0 when a Memory Card file copy is performed
 * from the PlayStation Memory Card set-up screen.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param block Block number of header (1-15)
 * @param exec 1: PDA application flag set, 0: Flag cancelled
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxExecFlag(int port, int block, int exec);

/**
 * @brief Set PDA application's flash memory write priority (process
 * registration)
 *
 * Controls whether the PDA application can write to flash memory. While a PDA
 * application is writing to flash memory, communication with the PlayStation
 * can lead to processing load and access contention problems.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param mode 0: Allow flash memory write, 1: Disable flash memory write
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxFlashAcs(int port, int mode);

/**
 * @brief Get executing PDA application number (process registration)
 *
 * Gets the block number of the header which contains the currently executing
 * PDA application. If the currently executing application is the "Start-up
 * Application", *aplno returns 0.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param aplno Pointer to store application number
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxGetApl(int port, int* aplno);

/**
 * @brief Read the contents of PDA memory (process registration)
 *
 * The specified number of bytes of PDA memory are read from the specified
 * start address and placed in the buffer.
 *
 * Readable areas: 0x0******, 0x2******, 0x4******, 0x6******, 0x8******,
 *                 0xA******, 0xB******, 0xC******, 0xD******
 *
 * Note: Accessing address 0x2****** without specifying virtual flash memory
 * will generate a bus error on the PDA.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param data Buffer for storing PDA memory read data
 * @param start Read start address
 * @param len Read size in bytes (128 bytes max)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxGetMem(int port, unsigned char* data, unsigned start, unsigned len);

/**
 * @brief Get PDA serial number (process registration)
 *
 * Gets the serial number of the PDA. The serial number can also be obtained
 * using McxAllInfo().
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param serial Pointer to store serial number (4 bytes)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxGetSerial(int port, unsigned long* serial);

/**
 * @brief Get PDA real-time clock (process registration)
 *
 * Obtains the date and time from the PDA's real-time clock. Information
 * other than day of the week are returned as BCD values.
 *
 * Buffer format (8 bytes):
 * - Offset 0: 100 years
 * - Offset 1: Year
 * - Offset 2: Month
 * - Offset 3: Date
 * - Offset 4: Day of week (0=Sun, 1=Mon, ..., 6=Sat)
 * - Offset 5: Hours
 * - Offset 6: Minutes
 * - Offset 7: Seconds
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param time Pointer to store time data (8 bytes)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxGetTime(int port, unsigned char* time);

/**
 * @brief Get user interface status (process registration)
 *
 * Obtains user interface status from the PDA.
 *
 * Buffer format (8 bytes):
 * - Offset 0: Alarm time (minutes, BCD)
 * - Offset 1: Alarm time (hours, BCD)
 * - Offset 2: Status bits
 *   - Bit 7: RTC set (0=unset/invalid, 1=set/valid)
 *   - Bit 6-4: Area code (0=Japan, 1=North America, 2=Europe)
 *   - Bit 3-2: Speaker volume (0=loud, 1=soft, 2=off)
 *   - Bit 1: Key lock (0=unlocked, 1=locked)
 *   - Bit 0: Alarm (0=off, 1=on)
 * - Offset 3: Unused
 * - Offset 4-5: Font data start address (LSB, MSB, relative to 0x4000000)
 * - Offset 6-7: Unused
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param data Pointer to UIFS data buffer (8 bytes)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxGetUIFS(int port, unsigned char* data);

/**
 * @brief Hide the "data transfer" display (process registration)
 *
 * Hides the "data transfer" display on the LCD screen made visible by
 * McxShowTrans(). When this function is called, a file transfer control
 * callback is generated from the PDA kernel to the currently executing
 * PDA application.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxHideTrans(int port);

/**
 * @brief Read from PDA device (process registration)
 *
 * Performs a read from a user-defined device or from a reserved device
 * provided on the PDA.
 *
 * Reserved devices:
 * - 0: RTC read/write
 * - 1: PDA memory read/write
 * - 2: User interface status read/write
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param dev Device number
 * @param param Parameter passed to device
 * @param data Data read from device
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxReadDev(int port, int dev, unsigned char* param, unsigned char* data);

/**
 * @brief Switch the LED on/off (process registration)
 *
 * Turns the LED on and off. McxGetMem() and McxSetMem() can also be used
 * to check/set the on/off state of an LED by directly accessing PIO0 and PIO1.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param mode 0: Turn LED off, other values: Turn LED on
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxSetLED(int port, int mode);

/**
 * @brief Update the contents of PDA memory (process registration)
 *
 * Overwrites the specified number of bytes of PDA memory starting at the
 * specified address.
 *
 * Writable areas: 0x0******, 0x6******, 0xA******, 0xB******, 0xC******,
 *                 0xD******
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param data Buffer containing data to be written to PDA memory
 * @param start Write start address
 * @param len Write size in bytes (128 bytes max)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxSetMem(int port, unsigned char* data, unsigned start, unsigned len);

/**
 * @brief Set PDA real-time clock (process registration)
 *
 * Sets the date and time in the PDA's real-time clock. After communication
 * with the PDA completes, 150ms (worst case) are required for the RTC to
 * be set. During this interval, communication with the PDA is not possible.
 *
 * Buffer format (8 bytes, BCD except day of week):
 * - Offset 0: 100 years
 * - Offset 1: Year
 * - Offset 2: Month
 * - Offset 3: Date
 * - Offset 4: Day of week (0=Sun, 1=Mon, ..., 6=Sat)
 * - Offset 5: Hours
 * - Offset 6: Minutes
 * - Offset 7: Seconds
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param time Pointer to time data (8 bytes)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxSetTime(int port, unsigned char* time);

/**
 * @brief Set user interface status (process registration)
 *
 * Sets the PDA's user interface status.
 *
 * Buffer format (8 bytes):
 * - Offset 0: Alarm time (minutes, BCD)
 * - Offset 1: Alarm time (hours, BCD)
 * - Offset 2: Status bits
 *   - Bit 7: RTC set (0=unset/invalid, 1=set/valid)
 *   - Bit 6-4: Area code (0=Japan, 1=North America, 2=Europe)
 *   - Bit 3-2: Speaker volume (0=loud, 1=soft, 2=off)
 *   - Bit 1: Key lock (0=unlocked, 1=locked)
 *   - Bit 0: Alarm (0=off, 1=on)
 * - Offset 3: Unused
 * - Offset 4-5: Font data start address (LSB, MSB, relative to 0x4000000)
 * - Offset 6-7: Unused
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param data Pointer to UIFS data (8 bytes)
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxSetUIFS(int port, unsigned char* data);

/**
 * @brief Show the "data transfer" display (process registration)
 *
 * Shows the "data transfer" display on the PDA's LCD screen. Call this
 * function before opening a file to avoid alternate sector processing when
 * saving a PDA application file or a data file that contains a PDA file
 * list icon.
 *
 * Note: Do not call this function when formatting the Memory Card, as
 * MemCardFormat() and _card_format() initialize alternate sectors.
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param dir Transfer direction (0: PDA->PS, other values: PS->PDA)
 * @param timeout Timeout in seconds until display is auto-cleared
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxShowTrans(int port, int dir, int timeout);

/**
 * @brief Start the PDA system
 *
 * Starts the PDA system. Enables PDA processing-related interrupts and allows
 * each of the PDA's process registration functions to be used.
 *
 * Calling sequence (functions at arrow start must be called first):
 *   InitCARD() -> StartCARD() -> McxStartCom()
 *   or
 *   MemCardInit() -> MemCardStart() -> McxStartCom()
 *
 * Standard sequence:
 *   PadInit();
 *   InitPAD(); StartPAD();
 *   InitCARD(); StartCARD();
 *   McxStartCom();
 *   PadInitDirect(); PadStartCom();
 */
void McxStartCom(void);

/**
 * @brief Stop the PDA system
 *
 * Halts PDA system-related interrupts and shuts down the PDA system.
 * McxStartCom() and McxStopCom() must be correctly nested with the
 * calling sequence of other start/stop function pairs.
 */
void McxStopCom(void);

/**
 * @brief Confirm the completion of a registered process
 *
 * Checks the progress of a registered process. The command number of the
 * executing or completed process is returned in cmd. The processing result
 * is returned in result.
 *
 * @param mode 0: Wait until registered process completes,
 *             1: Check current state and return immediately
 * @param cmd Pointer to receive completed command number (McxFunc* macros)
 * @param result Pointer to receive result code (McxErr* macros)
 * @return McxSyncRun (0): Processing,
 *         McxSyncFin (1): Process complete,
 *         McxSyncNone (-1): Process unregistered
 */
int McxSync(int mode, long* cmd, long* result);

/**
 * @brief Write to PDA device (process registration)
 *
 * Performs a write to a user-defined device or to a reserved device
 * provided on the PDA.
 *
 * Reserved devices:
 * - 0: RTC read/write
 * - 1: PDA memory read/write
 * - 2: User interface status read/write
 *
 * @param port Port number (0x00-0x03 for Port 1, 0x10-0x13 for Port 2)
 * @param dev Device number
 * @param param Parameter passed to device
 * @param data Data to write to device
 * @return 1 if process registration was accepted, 0 if registration failed
 */
int McxWriteDev(int port, int dev, unsigned char* param, unsigned char* data);

#endif /* LIBMCX_H */
