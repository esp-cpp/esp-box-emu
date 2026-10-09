#ifndef _LIBTAP_H_
#define _LIBTAP_H_

/**
 * @brief Initialize controller
 *
 * Registers a receive data buffer for the controller.
 *
 * @param bufA Pointer to receive data buffer
 * @param lenA Receive data buffer length (unit: byte)
 * @param bufB Pointer to receive data buffer
 * @param lenB Receive data buffer length (unit: byte)
 * @return 1 if successful
 */
int InitTAP(char* bufA, long lenA, char* bufB, long lenB);

/**
 * @brief Start controller reading
 *
 * Starts controller reading at Vsync interrupt
 */
void StartTAP(void);

/**
 * @brief Halt controller reading
 *
 * Halts the controller reading. Does not prohibit interrupts.
 */
void StopTAP(void);

/**
 * @brief undocumented
 */
long SendTAP(char*, long, char*, long);

/**
 * @brief Enables occurrence of an event.
 *
 * Enables communication with a controller which was disabled with DisableTAP().
 *
 * Although a normal controller communicates via Vsync interrupts, this function
 * is used only with timing longer than 1/60 sec when the controller status is
 * not needed
 */
void EnableTAP(void);

/**
 * @brief Disable communication with the controller
 *
 * Temporarily disables communication with the controller.
 *
 * Although StopTAP() deletes the controller handler activated by Vsync
 * interrupts, this function simply skips controller communication with a flag
 * operation.
 *
 * Although a normal controller communicates via Vsync interrupts,
 * this function is used only with timing longer than 1/60 sec when the
 * controller status is not needed.
 */
void DisableTAP(void);

#endif
