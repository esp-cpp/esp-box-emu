#ifndef PSYZ_INPUT_H
#define PSYZ_INPUT_H

/**
 * @file input.h
 * @brief Controller and gamepad input endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Controller identifiers to emulate multiple PS1 controller kinds
 *
 * https://problemkaputt.de/psxspx-controllers-communication-sequence.htm
 */
typedef enum {
    PSYZ_CTRL_ERROR = 0x00,         /**< Failed to set controller param */
    PSYZ_CTRL_QUERY_KIND = 0x00,    /**< Query current kind without changing it */
    PSYZ_CTRL_MOUSE = 0x12,         /**< TODO document buffer format */
    PSYZ_CTRL_DIGITAL_PAD = 0x41,   /**< TODO document buffer format */
    PSYZ_CTRL_ANALOG_STICK = 0x53,  /**< TODO document buffer format */
    PSYZ_CTRL_ANALOG_PAD = 0x73,    /**< TODO document buffer format */
    PSYZ_CTRL_KEYBOARD = 0x96,      /**< TODO document buffer format */
    PSYZ_CTRL_DISCONNECTED = 0xFF,  /**< TODO */
} PsyzControllerKind;

/**
 * @brief Select which controller kind a port emulates
 *
 * Sets the controller kind reported on @p port and @p channel.
 * Defaults to PSYZ_CTRL_DIGITAL_PAD for better compatibility with early games.
 * When the reserved PSYZ_CTRL_QUERY_KIND value is passed, fetch the current
 * controller kind for the specified @p port and @p channel as returned value.
 *
 * @param port Controller port (0 or 1)
 * @param channel Multitap channel (0-3). Currently reserved and unused.
 * @param kind Controller kind to emulate, or PSYZ_CTRL_QUERY_KIND
 * @return Previously selected kind, or PSYZ_CTRL_ERROR on invalid arguments
 */
PsyzControllerKind Psyz_PadsSetKind(
    int port, int channel, PsyzControllerKind kind);

/**
 * Size of the controller receive buffer the BIOS fills on real hardware:
 * byte 0 = status, byte 1 = controller kind, then the per-kind payload.
 * https://problemkaputt.de/psxspx-controllers-and-memory-cards.htm
 */
#define PSYZ_PAD_BUF_LEN 34

/**
 * @brief Retrieve the internal 34-byte controller frame for a port
 *
 * Copies the latest hardware-faithful SIO frame for @p port into @p dst.
 * Direct low-level entrypoint for libetc/libapi. Depending on the backend
 * implementation, triggers an OS poll of gamepad or mapped keyboard.
 *
 * @param port Controller port (0 or 1)
 * @param dst Buffer to write into
 * @param len Buffer length in bytes (at most PSYZ_PAD_BUF_LEN is copied)
 */
void Psyz_PadsGet(int port, char* dst, int len);

/**
 * @brief Set the internal 34-byte controller frame.
 *
 * Called by the platform input backend to store the same frame retrieved with
 * Psyz_PadsGet. It can be used to emulate input as part of automated tests.
 *
 * @param port Controller port (0 or 1)
 * @param src Buffer to write from
 * @param len Buffer length in bytes
 */
void Psyz_PadsSet(int port, const char* src, int len);

#ifdef __cplusplus
}
#endif

#endif
