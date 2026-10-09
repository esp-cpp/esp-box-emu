#ifndef PSYZ_VIDEO_H
#define PSYZ_VIDEO_H

/**
 * @file video.h
 * @brief Window, display timing and frame presentation endpoints.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set the title of the game window
 *
 * @param str Window title string
 */
void Psyz_SetTitle(const char* str);

typedef enum {
    /**
     * Auto-detect use of driver VSync (default)
     * - Tests monitor refresh rate on initialization (adds ~30ms startup time)
     * - Uses driver VSync if monitor is detected between 57Hz-63Hz
     *   (±5% tolerance). This covers both 59.94Hz (NTSC) and 60Hz
     * - Uses manual limiter otherwise (144Hz, 165Hz, etc.)
     * - Balances performance and accuracy automatically
     */
    PSYZ_VSYNC_AUTO,

    /**
     * Always use driver VSync
     * Pros: Zero CPU overhead and no frame pacing on matching refresh rates
     * Cons: Game will run faster on high refresh monitors
     * Cons: Framerate always matches monitor exactly.
     *   On 60Hz monitor: runs at 60fps instead of NTSC 59.94fps
     *   causing ~1 second drift every ~16.7 minutes of gameplay
     * Real NTSC PSX hardware runs at 59.94Hz (60/1.001)
     *   PAL PSX hardware runs at exactly 50Hz
     * Best when combined with driver control panel frame rate limit
     * IMPORTANT: PAL games (50fps) will run at 60fps on 60Hz monitors without
     *   driver-level limiting, which is incorrect
     */
    PSYZ_VSYNC_ON,

    /**
     * Always use internal manual frame limiter
     * Pros: Precise 59.94fps (NTSC) / 50fps (PAL) matching real hardware
     * Pros: Safe for VRR displays
     * Pros: Consistent timing across all monitor refresh rates
     * Cons: ~6% CPU usage on one core (1ms busy-wait per frame for precision)
     * Cons: May have minor frame pacing variance on non-VRR displays
     */
    PSYZ_VSYNC_OFF,

    /**
     * Disable both driver VSync and the internal frame limiter.
     * Most games will be unplayable with this option, but it's useful for:
     *   - Adapting game on variable framerate using a deltaTime.
     *   - Reproduce gameplay with simulatd input on CI at high speed.
     *   - Measure frame time end-to-end and if game is CPU or GPU bound.
     */
    PSYZ_VSYNC_LIMITLESS,
} PsyzVsyncMode;

typedef enum {
    PSYZ_DITHER_AUTO, /**< let the game decide whether to dither (default) */
    PSYZ_DITHER_OFF,  /**< force dithering always off */
} PsyzDitherMode;

typedef enum {
    PSYZ_ASPECT_DISPLAY, /**< aspect from PS1 H/V display ranges (default) */
    PSYZ_ASPECT_SQUARE,  /**< 1:1 from framebuffer (pixel-perfect) */
} PsyzAspectMode;

typedef struct {
    double last_frame_time_us;       /**< duration of last frame */
    double last_draw_time_us;        /**< render time excluding vsync wait */
    double target_frame_time_us;     /**< target frame time */
    unsigned long long total_frames; /**< total frames rendered */
    int using_driver_vsync;          /**< 1 for VSync, 0 for limiter */
} PsyzVideoStats;

/**
 * @brief Set VSync mode (default: AUTO)
 *
 * @param mode VSync mode to set
 * @return 0 on success, -1 if invalid mode
 */
int Psyz_VideoSetVsyncMode(PsyzVsyncMode mode);

/**
 * @brief Set dithering mode (default: AUTO)
 *
 * @param mode Dithering mode to set
 * @return 0 on success, -1 if invalid mode
 */
int Psyz_VideoSetDitheringMode(PsyzDitherMode mode);

/**
 * @brief Select how the presented aspect ratio is determined
 *
 * DISPLAY: use the game horizontal/vertical display (display sync) ranges,
 *   so different game resolutions present at the intended physical proportions.
 *   This is the accurate and default behaviour, originally intended to control
 *   the beam range of a CRT display. This is not pixel-perfect.
 * SQUARE: present framebuffer pixels 1:1, ignoring sync ranges. This presents
 *   the game pixel-perfect to the display at the cost of intent-accuracy.
 *
 * @param mode Aspect mode to set
 * @return 0 on success, -1 if invalid mode
 */
int Psyz_VideoSetAspectMode(PsyzAspectMode mode);

/** Maximum accepted internal resolution multiplier */
#define PSYZ_INTERNAL_RES_MAX 8

/**
 * @brief Set the internal rendering resolution multiplier (default: 1)
 *
 * Internally multiplies horizontal and vertical resolution, giving 3D
 * geometry more sub-pixel precision (smoother edges, less shimmer).
 * Safe to call at runtime; the change applies on the next presented frame
 * without needing to restart the game.
 *
 * @param multiplier Integer scale factor, between 1 to PSYZ_INTERNAL_RES_MAX
 * @return 0 on success, -1 if multiplier < 1
 */
int Psyz_VideoSetInternalResolution(unsigned multiplier);

/**
 * @brief Get the current internal resolution multiplier
 *
 * @return current multiplier (>= 1)
 */
unsigned Psyz_VideoGetInternalResolution(void);

/**
 * @brief Synchronize with vertical blank
 *
 * Synchronize with the refresh rate mode set in Psyz_VideoSetVsyncMode.
 * The interface is very similar to libetc VSync.
 *
 * @param mode Synchronization mode:
 *             - 0: Wait for next vertical blank
 *             - Negative: Return immediately (non-blocking)
 *             - Positive: Wait for specified number of vertical blanks
 * @return Number of vertical blanks since last call
 */
int Psyz_VideoVSync(int mode);

/**
 * @brief Get frame timing statistics
 *
 * @param stats Output structure to fill
 * @return 0 on success, -1 if stats is NULL or platform not initialized
 */
int Psyz_VideoStats(PsyzVideoStats* stats);

/**
 * @brief Get frame output as a byte array
 *
 * This function is very slow.
 *
 * @param w Output frame width
 * @param h Output frame height
 * @return NULL on failure, otherwise ptr to be destroyed with free(ptr)
 */
unsigned char* Psyz_VideoAllocCapturedFrame(int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif
