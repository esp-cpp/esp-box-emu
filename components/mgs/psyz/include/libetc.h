#ifndef LIBETC_H
#define LIBETC_H

/**
 * @file libetc.h
 * @brief Utility Library
 *
 * This library provides various utility functions for system control,
 * video mode management, controller input, and debugging support.
 *
 * Key features:
 * - VSync and timing control
 * - Video mode switching (NTSC/PAL)
 * - Controller/PAD input reading
 * - Callback management (VSync, DMA, Reset, Stop)
 * - Debug font and print functions
 * - BIOS call access
 * - System information retrieval
 */

extern int PadIdentifier;
/*
 * PAD I/O (SIO Pad)
 */
#define PADLup     (1<<12) // Up
#define PADLdown   (1<<14) // Down
#define PADLleft   (1<<15) // Left
#define PADLright  (1<<13) // Right
#define PADRup     (1<< 4) // Triangle
#define PADRdown   (1<< 6) // Cross
#define PADRleft   (1<< 7) // Square
#define PADRright  (1<< 5) // Circle
#define PADi       (1<< 9) // L3
#define PADj       (1<<10) // R3
#define PADk       (1<< 8) // Select
#define PADl       (1<< 3) // R1
#define PADm       (1<< 1) // R2
#define PADn       (1<< 2) // L1
#define PADo       (1<< 0) // L2
#define PADh       (1<<11) // Start
#define PADL1      PADn
#define PADL2      PADo
#define PADR1      PADl
#define PADR2      PADm
#define PADstart   PADh
#define PADselect  PADk

#define _PAD(x, y) ((y) << ((x) << 4))

/* Mouse button definitions */
#define MOUSEleft (1 << 3)  /**< Left mouse button */
#define MOUSEright (1 << 2) /**< Right mouse button */

/* Video modes */
#define MODE_NTSC 0 /**< NTSC video mode */
#define MODE_PAL 1  /**< PAL video mode */

/**
 * @brief System callbacks structure
 */
struct Callbacks {
    void (*unk0)(void);
    void* (*DMACallback)(int dma, void (*func)());
    void (*unk8)(void);
    int (*ResetCallback)(void);
    int (*StopCallback)(void);
    int (*VSyncCallbacks)(int ch, void (*f)());
    int (*RestartCallback)(void);
};

/*
 * VSync and timing functions
 */

/**
 * @brief Synchronize with vertical blank.
 *
 * Waits for vertical blank synchronization based on the mode parameter.
 *
 * @param mode Synchronization mode:
 *             - 0: Wait for next vertical blank
 *             - Negative: Return immediately (non-blocking)
 *             - Positive: Wait for specified number of vertical blanks
 * @return Number of vertical blanks since last call
 */
int VSync(int mode);

/**
 * @brief Register a VSync callback function.
 *
 * Registers a callback function to be executed at vertical blank interrupt.
 * This is equivalent to VSyncCallbacks(0, f).
 *
 * @param f Pointer to callback function (NULL to unregister)
 * @return Previous callback function pointer
 */
int VSyncCallback(void (*f)());

/**
 * @brief Register VSync callback for specific channel.
 *
 * Registers a callback function to be executed at vertical blank interrupt
 * on a specific channel. Multiple channels allow multiple callbacks.
 *
 * @param ch Channel number (0-3)
 * @param f Pointer to callback function (NULL to unregister)
 * @return Previous callback function pointer for the channel
 */
int VSyncCallbacks(int ch, void (*f)());

/*
 * Callback management
 */

/**
 * @brief Register DMA callback function.
 *
 * Registers a callback function for DMA channel interrupts.
 *
 * @param dma DMA channel number
 * @param func Pointer to callback function (NULL to unregister)
 * @return Previous callback function pointer
 */
void* DMACallback(int dma, void (*func)());

/**
 * @brief Initialize all callbacks.
 *
 * Initializes all system callbacks, setting all callback functions to
 * unregistered and setting up the interrupt context stack. Must be called
 * after program boot before any other processing.
 *
 * The environment remains valid until StopCallback() is called.
 * Subsequent calls to ResetCallback() are ignored.
 */
int ResetCallback(void);

/**
 * @brief Stop all callbacks.
 *
 * Stops all system callbacks. Should be called before terminating programs
 * to disable all interrupts.
 */
int StopCallback(void);

/**
 * @brief Restart a halted callback.
 *
 * Restores the halted callback to the status immediately prior to when it
 * was halted. Unlike ResetCallback(), this does not initialize callback
 * functions or the callback stack. ResetCallback() must be executed before
 * calling RestartCallback().
 *
 * @return Used by system only
 */
int RestartCallback(void);

/**
 * @brief Determine callback execution context.
 *
 * Determines whether the program is currently executing in callback context
 * or normal context.
 *
 * @return 0: normal context; 1: callback context
 */
int CheckCallback(void);

/*
 * Video mode functions
 */

/**
 * @brief Get present video signaling system.
 *
 * Returns the video signaling system set by SetVideoMode(). If SetVideoMode()
 * wasn't called, returns MODE_NTSC regardless of the actual machine type.
 *
 * @return Video signaling system mode (MODE_NTSC or MODE_PAL)
 */
long GetVideoMode(void);

/**
 * @brief Declare current video signaling system.
 *
 * Declares the video signaling system to the libraries. Related libraries
 * will conform to the declared video signaling system environment.
 * Should be called before all library functions.
 *
 * @param mode Video signaling system mode (MODE_NTSC or MODE_PAL)
 * @return Previously-set video signaling system mode
 */
long SetVideoMode(long mode);

/*
 * PAD/Controller functions - Basic (for prototyping only)
 */

/**
 * @brief Initialize a controller (for prototyping only).
 *
 * Initializes the controller. Since this function supports only the 16-button
 * controller, it should be used for prototyping purposes only.
 *
 * @param mode Always pass 0
 */
void PadInit(int mode);

/**
 * @brief Read data from the controller (for prototyping only).
 *
 * Reads data from the controller. This function is for prototyping purposes
 * only.
 *
 * @param id Controller ID (unused)
 * @return Controller button status. High 2 bytes are pad 2, low 2 bytes are
 * pad 1.
 */
unsigned int PadRead(int id);

/**
 * @brief Halt controller (for prototyping only).
 *
 * Halts all currently connected controllers. Must be called when processing is
 * complete to halt the controller driver. For prototyping purposes only.
 */
void PadStop(void);

/*
 * PAD/Controller functions - Advanced (libpad)
 */

/**
 * @brief Initialize controller environment (for direct connection).
 *
 * Initializes the control environment for a controller directly connected to
 * the main PlayStation unit. Cannot be used with PadInitMtap(), InitPAD(),
 * InitGUN(), InitTAP(), or PadInit().
 *
 * @param pad1 Port 1 receive results buffer (34 bytes)
 * @param pad2 Port 2 receive results buffer (34 bytes)
 */
void PadInitDirect(unsigned char* pad1, unsigned char* pad2);

/**
 * @brief Initialize controller environment (for Multi Taps).
 *
 * Initializes the control environment for a controller. If a Multi Tap is
 * connected, it is treated as a Multi Tap. Cannot be used with PadInitDirect(),
 * InitPAD(), InitGUN(), InitTAP(), or PadInit().
 *
 * Note: A Multi Tap may not be recognized if a controller is not connected to
 * port A. Always connect a controller to port A of the Multi Tap.
 *
 * @param pad1 Port 1 receive results buffer (34 bytes)
 * @param pad2 Port 2 receive results buffer (34 bytes)
 */
void PadInitMtap(unsigned char* pad1, unsigned char* pad2);

/**
 * @brief Initialize controller environment (for guns using interrupts).
 *
 * Sets up the horizontal/vertical position receive buffer for light guns.
 * Retrieval is triggered by an interrupt from the gun. Must be called after
 * PadInitDirect() or PadInitMtap().
 *
 * @param buff Horizontal/vertical position receive buffer (size*4+2 bytes)
 * @param size Maximum number of gun interrupts per 1Vsync (maximum 20)
 */
void PadInitGun(unsigned char* buff, int size);

/**
 * @brief Start reading from controller.
 *
 * Initiates a controller read operation triggered by vertical retrace interval
 * interrupt.
 */
void PadStartCom(void);

/**
 * @brief Stop controller read.
 *
 * Stops a controller read operation. Stops handling all vertical interval
 * interrupts related to controller services.
 */
void PadStopCom(void);

/**
 * @brief Enable/disable communication with the controller.
 *
 * Communication normally occurs once per frame (1/60th second). This function
 * temporarily disables communication to provide greater processing time. Only
 * works between PadStartCom() and PadStopCom().
 *
 * If communication is suspended for 3+ seconds, the controller is reset.
 *
 * @param mode Bit 0: enable/disable port 0, Bit 1: enable/disable port 1.
 *             1 = enabled; 0 = disabled
 * @return Previous enable/disable state before the function was called
 */
unsigned PadEnableCom(unsigned mode);

/**
 * @brief Check communication with controller.
 *
 * Determines whether communication with the controller has occurred in a frame.
 * Should be called once per frame during Vsync.
 *
 * @return 1: Communication took place (regardless of success/failure)
 *         0: Communication did not take place (or function called twice or more
 * in a frame)
 */
int PadChkVsync(void);

/**
 * @brief Get controller connection state.
 *
 * Checks controller connection status, determines when button-press information
 * is valid, and determines when actuator information is valid.
 *
 * @param port Port number to check (0x00-0x03 for port 1, 0x10-0x13 for port 2)
 *             - Direct connection: 0x00, 0x10
 *             - Multi Tap A-D: 0x00-0x03, 0x10-0x13
 * @return Connection state:
 *         - 0 (PadStateDiscon): Controller disconnected
 *         - 1 (PadStateFindPad): Find controller connection (checking)
 *         - 2 (PadStateFindCTP1): Controller without vibration connected
 *         - 4 (PadStateReqInfo): Actuator information being retrieved
 *         - 5 (PadStateExecCmd): Library communicating with controller
 *         - 6 (PadStateStable): Controller with vibration ready, info complete
 */
int PadGetState(int port);

/**
 * @brief Get actuator information.
 *
 * Obtains actuator function number, sub-function number, parameter data size,
 * and current drain. Returns 0 for controllers without vibration support.
 *
 * @param port Port number of controller (0x00-0x03 for port 1, 0x10-0x13 for
 * port 2)
 * @param actno Actuator number (0 to total-1), or -1 to get total number of
 * actuators
 * @param term Information to check:
 *             - 1 (InfoActFunc): Function number
 *             - 2 (InfoActSub): Sub-function number
 *             - 3 (InfoActSize): Parameter data length
 *             - 4 (InfoActCurr): Maximum current drain
 * @return Requested information, or 0 if invalid
 */
int PadInfoAct(int port, int actno, int term);

/**
 * @brief Get information on actuator combinations.
 *
 * Checks combinations of actuators that can be used simultaneously based on
 * physical arrangement restrictions.
 *
 * @param port Port number of controller (0x00-0x03 for port 1, 0x10-0x13 for
 * port 2)
 * @param listno List number (0 to total-1), or -1 to get total number of lists
 * @param offs Offset within list (0 to total-1), or -1 to get total actuators
 * in list
 * @return Requested information, or 0 if invalid
 */
int PadInfoComb(int port, int listno, int offs);

/**
 * @brief Get information about controller mode.
 *
 * Checks currently active controller mode ID, distinguishes DUAL SHOCK from
 * other controllers, and checks supported controller mode IDs.
 *
 * @param port Port number of controller (0x00-0x03 for port 1, 0x10-0x13 for
 * port 2)
 * @param term Item to check:
 *             - 1 (InfoModeCurID): Currently active controller mode ID
 *             - 2 (InfoModeCurExID): Active mode ID on vibration controller
 *             - 3 (InfoModeCurExOffs): Offset in mode ID table
 *             - 4 (InfoModeIdTable): Mode ID at offset specified by offs
 * @param offs Offset in controller mode ID table (ignored unless term=4)
 * @return Requested information, or 0 if invalid
 */
int PadInfoMode(int port, int term, int offs);

/**
 * @brief Set transmit buffer.
 *
 * Registers the transmit data buffer in the library. When actuator operation
 * changes and buffer contents change, the library reads the buffer every Vsync
 * and automatically transmits to the controller.
 *
 * @param port Target port number (0x00-0x03 for port 1, 0x10-0x13 for port 2)
 * @param data Transmit data buffer
 * @param len Length of transmit data buffer (in bytes)
 */
void PadSetAct(int port, unsigned char* data, int len);

/**
 * @brief Set actuator parameter details.
 *
 * Indicates to the controller the position in the transmit buffer where
 * actuator parameters are located by writing actuator numbers in appropriate
 * positions. Request is rejected if library is communicating with controller.
 *
 * @param port Port number of controller (0x00-0x03 for port 1, 0x10-0x13 for
 * port 2)
 * @param data Actuator parameter transmission details (6 bytes)
 * @return 1 if request accepted, 0 if rejected
 */
int PadSetActAlign(int port, char* data);

/**
 * @brief Switch/lock the controller mode selector.
 *
 * Selects the controller mode and switches between locked and unlocked settings
 * for the controller mode selection button. Request is rejected if library is
 * communicating with controller.
 *
 * @param port Port number (0x00-0x03 for port 1, 0x10-0x13 for port 2)
 * @param offs Controller mode ID table offset containing the mode to switch to
 * @param lock Bit 1: 0=keep current state, 1=change state
 *             Bit 0: 0=unlock selector, 1=lock selector (only if bit 1 is 1)
 * @return 1 if request accepted, 0 if rejected
 */
int PadSetMainMode(int port, int offs, int lock);

/**
 * @brief Enable/disable gun interrupts.
 *
 * Enables gun interrupts when corresponding mask bit is set to 1.
 * Default is disabled for all ports. Retrieval of position information begins
 * when a gun is connected.
 *
 * @param mask Enable gun interrupts for specific ports (bit D0-D7 for ports
 *             0x00-0x03, 0x10-0x13)
 */
void PadEnableGun(unsigned char mask);

/**
 * @brief Stop retrieval of horizontal/vertical gun position.
 *
 * Stops the retrieval of horizontal/vertical gun position information.
 */
void PadRemoveGun(void);

/*
 * TAP/Multi Tap functions (libtap)
 */

/**
 * @brief Initialize controller (Multi Tap).
 *
 * Registers a receive data buffer for the controller. See Library Overview
 * documentation for receive buffer data format.
 *
 * @param bufA Pointer to receive data buffer
 * @param lenA Receive data buffer length (unit: byte)
 * @param bufB Pointer to receive data buffer
 * @param lenB Receive data buffer length (unit: byte)
 */
int InitTAP(char* bufA, long lenA, char* bufB, long lenB);

/**
 * @brief Start controller reading.
 *
 * Starts controller reading at Vsync interrupt.
 */
void StartTAP(void);

/**
 * @brief Halt controller reading.
 *
 * Halts controller reading. Does not prohibit interrupts.
 */
void StopTAP(void);

/**
 * @brief Enable communication with controller.
 *
 * Enables communication with a controller which was disabled with DisableTAP().
 */
void EnableTAP(void);

/**
 * @brief Disable communication with controller.
 *
 * Temporarily disables communication with the controller using a flag
 * operation. Used when controller status is not needed and timing is longer
 * than 1/60 sec.
 */
void DisableTAP(void);

/*
 * GUN/Light Gun functions (libgun)
 */

/**
 * @brief Initialize gun.
 *
 * Defines the buffers used to receive data from the light gun and other
 * controllers. Cannot be used at the same time as InitPAD() or InitTAP().
 *
 * As of library v4.0, DMA operations and interrupts are blocked within the
 * gun interrupt handler to improve accuracy.
 *
 * @param bufA Controller receive data buffer for port 0
 * @param lenA Length in bytes of bufA
 * @param bufB Controller receive data buffer for port 1
 * @param lenB Length in bytes of bufB
 * @param buf0 Pointer to horizontal/vertical position receive buffer
 * @param buf1 Pointer to horizontal/vertical position receive buffer
 *             (necessary buffer size is len*4+2 bytes)
 * @param len Number of gun interrupts allowed between vertical blank periods
 *            (20 maximum)
 */
void InitGUN(char* bufA, long lenA, char* bufB, long lenB, char* buf0,
             char* buf1, long len);

/**
 * @brief Start controller reading.
 *
 * Starts controller reading at Vsync interrupt.
 *
 * @return 1 if successful; 0 on failure
 */
long StartGUN(void);

/**
 * @brief Halt controller reading.
 *
 * Halts the controller reading. Does not prohibit interrupts.
 */
void StopGUN(void);

/**
 * @brief Select gun.
 *
 * Sets the interrupt mask for the gun. It is not possible to disable
 * interrupts for two masks at the same time.
 *
 * @param ch Gun channel (0 or 1)
 * @param mask Interrupt mask setting (0: interrupts prohibited, 1: interrupts
 * permitted)
 */
void SelectGUN(int ch, unsigned char mask);

/**
 * @brief Remove gun driver.
 *
 * Removes the gun driver registered in InitGUN().
 */
void RemoveGUN(void);

#endif
