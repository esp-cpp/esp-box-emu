#ifndef LIBCOMB_H
#define LIBCOMB_H

/**
 * @file libcomb.h
 * @brief Link Cable Library
 *
 * This library provides functions for serial communication using the
 * PlayStation link cable. It supports asynchronous read/write operations,
 * communication parameter configuration, and flow control.
 */

#include <psyz/types.h>

/* Driver Status bit definitions (Table 13-2) */
#define COMB_STAT_TX_RDY 0x0001   /**< Bit 0: Possible to write sending data */
#define COMB_STAT_RX_RDY 0x0002   /**< Bit 1: Possible to read receiving data */
#define COMB_STAT_TX_EMPTY 0x0004 /**< Bit 2: No sending data */
#define COMB_STAT_PARITY 0x0008   /**< Bit 3: Parity error occurrence */
#define COMB_STAT_OVERRUN 0x0010  /**< Bit 4: Overrun error occurrence */
#define COMB_STAT_FRAME 0x0020    /**< Bit 5: Frame error occurrence */
#define COMB_STAT_DSR 0x0080      /**< Bit 7: DSR is ON */
#define COMB_STAT_CTS 0x0100      /**< Bit 8: CTS is ON */
#define COMB_STAT_IRQ 0x0200      /**< Bit 9: Interrupt is ON */

/* Control Line Status bit definitions (Table 13-3) */
#define COMB_CTRL_DTR 0x0001 /**< Bit 0: DTR is ON */
#define COMB_CTRL_RTS 0x0002 /**< Bit 1: RTS is ON */

/* Communication Mode bit definitions (Table 13-4) */
#define COMB_MODE_CHAR_5 0x00     /**< Character length: 5 bits */
#define COMB_MODE_CHAR_6 0x04     /**< Character length: 6 bits */
#define COMB_MODE_CHAR_7 0x08     /**< Character length: 7 bits */
#define COMB_MODE_CHAR_8 0x0C     /**< Character length: 8 bits */
#define COMB_MODE_PARITY_EN 0x10  /**< Parity check enabled */
#define COMB_MODE_PARITY_ODD 0x20 /**< Odd parity (0 = even) */
#define COMB_MODE_STOP_1 0x40     /**< Stop bit length: 1 */
#define COMB_MODE_STOP_1_5 0x80   /**< Stop bit length: 1.5 */
#define COMB_MODE_STOP_2 0xC0     /**< Stop bit length: 2 */

/**
 * @brief Link cable driver control
 *
 * Offers the same functionality as ioctl() to an SIO device.
 * All the macros in this library are versions of this command.
 *
 * Command summary:
 * - cmd=0, arg=0: Returns serial controller status
 * - cmd=0, arg=1: Returns control line status
 * - cmd=0, arg=2: Returns communication mode
 * - cmd=0, arg=3: Returns communication rate in bps
 * - cmd=0, arg=4: Returns unit-number of characters for receiving
 * - cmd=0, arg=5: Returns remaining data bytes (param: 0=write, 1=read)
 * - cmd=0, arg=6: Returns async request status (param: 0=write, 1=read)
 * - cmd=1, arg=1: Sets control line status
 * - cmd=1, arg=3: Sets communication rate (bps)
 * - cmd=1, arg=4: Sets unit-number of characters for receiving
 * - cmd=2, arg=0: Resets serial controller
 * - cmd=2, arg=1: Clears driver status error bits
 * - cmd=2, arg=2: Cancels asynchronous writing
 * - cmd=2, arg=3: Cancels asynchronous reading
 * - cmd=3, arg=0: Sets RTS (param: 1=on, 0=off)
 * - cmd=3, arg=1: Returns CTS status
 * - cmd=4, arg=0: Registers wait callback function
 *
 * @param cmd Command
 * @param arg Subcommand
 * @param param Argument
 * @return Depends on the control command
 */
long _comb_control(u_long cmd, u_long arg, u_long param);

/**
 * @brief Initialize link cable driver
 *
 * Initializes the link cable driver and registers it with the kernel.
 */
void AddCOMB(void);

/**
 * @brief Clear interrupt from expanded SIO in the driver
 *
 * If val is non-0, an interrupt from an expansion SIO in the driver
 * is cleared. This is used only when other expansion SIO drivers are
 * also present.
 *
 * @param val Interrupt cause clear flag (non-0 to clear)
 */
void ChangeClearSIO(long val);

/**
 * @brief Remove link cable driver from kernel
 *
 * Removes the link cable driver from the kernel.
 */
void DelCOMB(void);

/**
 * @brief Get asynchronous communication request status
 *
 * Determines whether an asynchronous input/output request has been made.
 * Equivalent to _comb_control(0, 6, param).
 *
 * @param param 0 for asynchronous write, 1 for asynchronous read
 * @return 1 if request has been made; 0 otherwise
 */
#define CombAsyncRequest(param) _comb_control(0, 6, (param))

/**
 * @brief Get remaining transmit or receive data
 *
 * Gets the remaining data count from the asynchronous read or
 * asynchronous write being processed.
 * Equivalent to _comb_control(0, 5, param).
 *
 * @param param 0 for asynchronous write, 1 for asynchronous read
 * @return The number of bytes remaining
 */
#define CombBytesRemaining(param) _comb_control(0, 5, (param))

/**
 * @brief Get number of bytes left to receive
 *
 * Obtains the number of bytes left in the current asynchronous
 * read operation.
 * Equivalent to _comb_control(0, 5, 1).
 *
 * @return The number of bytes remaining
 */
#define CombBytesToRead() _comb_control(0, 5, 1)

/**
 * @brief Get number of bytes left to send
 *
 * Obtains the number of bytes remaining in the current asynchronous
 * write operation.
 * Equivalent to _comb_control(0, 5, 0).
 *
 * @return The number of bytes remaining
 */
#define CombBytesToWrite() _comb_control(0, 5, 0)

/**
 * @brief Cancel asynchronous read
 *
 * Cancels current asynchronous read operation.
 * Equivalent to _comb_control(2, 3, 0).
 *
 * @return 1
 */
#define CombCancelRead() _comb_control(2, 3, 0)

/**
 * @brief Cancel asynchronous write
 *
 * Cancels current asynchronous write operation.
 * Equivalent to _comb_control(2, 2, 0).
 *
 * @return 1
 */
#define CombCancelWrite() _comb_control(2, 2, 0)

/**
 * @brief Get control line status
 *
 * Obtains the control line status.
 * Equivalent to _comb_control(0, 1, 0).
 *
 * @return Control line status:
 *         - Bit 0: DTR is ON
 *         - Bit 1: RTS is ON
 */
#define CombControlStatus() _comb_control(0, 1, 0)

/**
 * @brief Get status of CTS signal
 *
 * Obtains the state of the serial controller CTS bit.
 * Equivalent to _comb_control(3, 1, 0).
 *
 * @return 1 if CTS is 1; 0 otherwise
 */
#define CombCTS() _comb_control(3, 1, 0)

/**
 * @brief Get communication speed
 *
 * Obtains the communication speed (in bps).
 * Equivalent to _comb_control(0, 3, 0).
 *
 * @return The communication speed (in bps)
 */
#define CombGetBPS() _comb_control(0, 3, 0)

/**
 * @brief Get communication mode
 *
 * Obtains the communication mode.
 * Equivalent to _comb_control(0, 2, 0).
 *
 * @return The communication mode:
 *         - Bits 7,6: Stop bit length (01=1, 10=1.5, 11=2)
 *         - Bit 5: Parity (1=odd, 0=even)
 *         - Bit 4: Parity enabled
 *         - Bits 3,2: Character length (00=5, 01=6, 10=7, 11=8 bits)
 *         - Bit 1: Always 1
 *         - Bit 0: Always 0
 */
#define CombGetMode() _comb_control(0, 2, 0)

/**
 * @brief Get receive packet size
 *
 * Obtains the receive packet size.
 * Equivalent to _comb_control(0, 4, 0).
 *
 * @return The receive packet size
 */
#define CombGetPacketSize() _comb_control(0, 4, 0)

/**
 * @brief Initialize the serial controller
 *
 * Initializes the serial controller. Controller status, communication
 * mode and communication speed remain unchanged.
 * Equivalent to _comb_control(2, 0, 0).
 *
 * @return 0
 */
#define CombReset() _comb_control(2, 0, 0)

/**
 * @brief Initialize error flags
 *
 * Clears error-related bits from driver status. Includes a function
 * which indicates the completion of the interrupt processing to the driver.
 * Equivalent to _comb_control(2, 1, 0).
 *
 * @return 0
 */
#define CombResetError() _comb_control(2, 1, 0)

/**
 * @brief Reset vertical blanking signal
 *
 * Resets the vertical blanking signal.
 * Equivalent to _comb_control(5, 0, 0).
 *
 * @return 0
 */
#define CombResetVBLANK() _comb_control(5, 0, 0)

/**
 * @brief Set communication speed
 *
 * Sets the communication speed. bps must be in the range 9600-2073600
 * and evenly divisible into 2073600. If asynchronous write is used,
 * the maximum communication speed is 57600 bps.
 * Equivalent to _comb_control(1, 3, bps).
 *
 * @param bps Communication speed (in bps)
 * @return 0
 */
#define CombSetBPS(bps) _comb_control(1, 3, (bps))

/**
 * @brief Set control line status
 *
 * Sets the control line status.
 * Equivalent to _comb_control(1, 1, val).
 *
 * @param val Control line status:
 *            - Bit 0: DTR on
 *            - Bit 1: RTS on
 * @return 0
 */
#define CombSetControl(val) _comb_control(1, 1, (val))

/**
 * @brief Set communication mode
 *
 * Sets the communication mode.
 * Equivalent to _comb_control(1, 2, mode).
 *
 * @param mode Communication mode:
 *             - Bits 7,6: Stop bit length (01=1, 10=1.5, 11=2)
 *             - Bit 5: Parity (1=odd, 0=even)
 *             - Bit 4: Parity enabled
 *             - Bits 3,2: Character length (00=5, 01=6, 10=7, 11=8 bits)
 *             - Bit 1: Always 1
 *             - Bit 0: Always 0
 * @return 1
 */
#define CombSetMode(mode) _comb_control(1, 2, (mode))

/**
 * @brief Set receive packet size
 *
 * Sets the receive packet size, which sets the byte count used for
 * generating interrupts in asynchronous communication. A large packet
 * size lowers the frequency of interrupts, improving system performance.
 *
 * Note: When sending data asynchronously, the packet size must be set
 * to 1, since only 1 byte can be sent at a time.
 * Equivalent to _comb_control(1, 4, size).
 *
 * @param size Packet size (1, 2, 4, or 8)
 * @return 0
 */
#define CombSetPacketSize(size) _comb_control(1, 4, (size))

/**
 * @brief Set RTS signal
 *
 * Sets the RTS bit in control line status to 1.
 * Equivalent to _comb_control(3, 0, 1).
 *
 * @return 0
 */
#define CombSetRTS() _comb_control(3, 0, 1)

/**
 * @brief Get serial controller status
 *
 * Obtains serial controller status.
 * Equivalent to _comb_control(0, 0, 0).
 *
 * @return Serial controller status:
 *         - Bit 0: Transmit data available
 *         - Bit 1: Receive data available
 *         - Bit 2: No data to transmit
 *         - Bit 3: Parity error generated
 *         - Bit 4: Overrun error generated
 *         - Bit 5: Frame error generated
 *         - Bit 7: DSR is on
 *         - Bit 8: CTS is on
 *         - Bit 9: Interrupts on
 */
#define CombSioStatus() _comb_control(0, 0, 0)

/**
 * @brief Set wait callback function
 *
 * Registers a pointer to the wait callback function.
 * Equivalent to _comb_control(4, 0, func).
 *
 * @param func Pointer to the wait callback function
 * @return The value of the previous callback function
 */
#define CombWaitCallback(func) _comb_control(4, 0, (u_long)(func))

#endif /* LIBCOMB_H */
