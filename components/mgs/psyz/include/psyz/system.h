#ifndef PSYZ_SYSTEM_H
#define PSYZ_SYSTEM_H

/**
 * @file system.h
 * @brief Host filesystem shims and PlayStation 1 path adjustment helpers.
 */

#ifdef __psyz // exclude when targeting the PSX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define open psyz_open
#define close psyz_close
#define lseek psyz_lseek
#define read psyz_read
#define write psyz_write
#define ioctl psyz_ioctl
#define undelete psyz_undelete
#define _get_errno psyz_get_errno

#ifdef _WIN32
int psyz_open(const char* devname, int flag, ...);
int psyz_close(int fd);
long psyz_lseek(int fd, long offset, int flag);
int psyz_read(int fd, void* buf, unsigned int n);
int psyz_write(int fd, const void* buf, unsigned int n);
long psyz_ioctl(long fd, long com, long arg);
#else
int psyz_open(const char* devname, int flag);
int psyz_close(int fd);
long psyz_lseek(long fd, long offset, long flag);
long psyz_read(long fd, void* buf, long n);
long psyz_write(long fd, void* buf, long n);
long psyz_ioctl(long fd, long com, long arg);
#endif

#ifdef _MSC_VER
#define __builtin_memcpy memcpy
#endif

/**
 * @brief Adjust a PlayStation 1 path to the host filesystem
 *
 * Handles memory card paths (bu00:, bu10:, etc.) and other special cases.
 * If a custom callback is set via Psyz_AdjustPathCB and returns >= 0, the
 * callback result is used; otherwise internal adjustment is applied. The
 * function is used internally when opening and creating files, or when
 * enumerating the list of files in the specified directory.
 *
 * @note The filename portion (after the last path separator) is automatically
 *       truncated to 19 characters to match PS1 DIRENTRY.name[20] which
 *       requires null-termination for compatibility with SDK string functions.
 *       This truncation is applied regardless of whether callback or internal
 *       adjustment is used.
 *
 * @param dst Destination buffer for adjusted path (must be valid)
 * @param src Source path to adjust (PlayStation 1 format)
 * @param maxlen Maximum length of destination buffer
 */
void Psyz_AdjustPath(char* dst, const char* src, int maxlen);

/**
 * @brief Set custom path adjustment callback
 *
 * The callback is invoked before internal path adjustment. Its return value:
 * - < 0: No adjustment done, fall back to PSY-Z internal adjustment
 * - >= 0: Number of bytes written to dst (adjustment successful)
 *
 * The callback receives:
 * - dst: Destination buffer for adjusted path
 * - src: Source path to adjust
 * - maxlen: Maximum length of destination buffer
 *
 * @param callback Path adjustment callback
 */
void Psyz_AdjustPathCB(int (*callback)(char* dst, const char* src, int maxlen));

/**
 * @brief Join two path components with the platform's path separator
 *
 * Automatically handles path separators to avoid double separators.
 *
 * @param left Left path component (modified in-place)
 * @param right Right path component to append
 * @param maxlen Maximum length of left buffer
 * @return Pointer to left on success, NULL if buffer too small
 */
char* Psyz_JoinPath(char* left, const char* right, int maxlen);

/**
 * @brief Callback fired once per emulated vsync.
 */
typedef void (*PsyzVSyncCb)(void);

/**
 * @brief Register a callback fired once per libapi VSync.
 *
 * Used to set custom VSync callbacks that do not replace the existing
 * VSyncCallback set by the game. The registered callback will be executed
 * right before the callback set with VSyncCallback.
 *
 * @param cb Callback to register, or NULL to unregister
 */
void Psyz_SetVSyncCb(PsyzVSyncCb cb);

#ifdef __cplusplus
}
#endif

#endif
