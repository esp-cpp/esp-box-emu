#ifndef MCGUI_H
#define MCGUI_H
#include <psyz/types.h>

/**
 * @file mcgui.h
 * @brief Memory Card GUI Module
 *
 * This module provides a complete graphical user interface for Memory Card
 * operations including loading and saving game data. It handles all user
 * interaction, screen rendering, sound effects, and Memory Card management.
 *
 * Key features:
 * - Complete GUI for load/save operations
 * - Customizable backgrounds, textures, and cursor
 * - BGM and sound effect support
 * - Controller support (standard, mouse, analog, NeGcon)
 * - Japanese and English language modes
 * - Automatic icon animation (1-3 frames)
 */

/**
 * @brief BG data structure
 *
 * Controls the background display on the Memory Card screen.
 */
typedef struct sMcGuiBg {
    short mode; /**< BG mode: 0=scroll (64x64 tiled), 1=still image (timadr) */
    signed char scrollDirect; /**< Scroll direction (mode=0): 0=up, 1=top-left,
                                 2=left, 3=bottom-left, 4=down, 5=bottom-right,
                                 6=right, 7=top-right */
    signed char scrollSpeed;  /**< Scroll speed (mode=0): 0=stopped, 1=1/60 sec,
                                 2=1/30 sec, 3=1/20 sec */
    u_long* timadr; /**< Header address of TIM data for BG (valid when mode=1)
                     */
} sMcGuiBg;

/**
 * @brief Memory Card data structure
 *
 * Holds information for loading and saving game data.
 */
typedef struct sMcGuiCards {
    char file[21];    /**< Filename (ASCII only, no 0x00/0x2a(*)/0x3f(?),
                         null-terminated) */
    char title[65];   /**< Document name (full-size 32 chars SJIS, non-kanji and
                         level-1 kanji, no 0x84bf-0x889e, null-terminated) */
    char frame;       /**< Reserved area (unusable) */
    char block;       /**< Number of icon images for auto-animation (1-3) */
    u_long* iconAddr; /**< TIM data header address for icon image */
    u_long* dataAddr; /**< Header address of game data */
    int dataBytes;    /**< Number of game data bytes (128-byte units) */
} sMcGuiCards;

/**
 * @brief Controller data structure
 *
 * Holds controller configuration for supported peripheral types.
 */
typedef struct sMcGuiController {
    volatile u_char* buf[2]; /**< Controller's receive data buffer */
    struct {
        int flag;             /**< 1: supported, 0: not supported */
        u_long BUTTON_OK;     /**< Button code for OK/confirm */
        u_long BUTTON_CANCEL; /**< Button code for cancel */
    } type1;                  /**< Default controller */
    struct {
        int flag;             /**< 1: supported, 0: not supported */
        u_long BUTTON_OK;     /**< Button code for OK/confirm */
        u_long BUTTON_CANCEL; /**< Button code for cancel */
    } type2;                  /**< Mouse */
    struct {
        int flag;             /**< 1: supported, 0: not supported */
        u_long BUTTON_OK;     /**< Button code for OK/confirm */
        u_long BUTTON_CANCEL; /**< Button code for cancel */
    } type3;                  /**< Analog joystick, DUAL SHOCK */
    struct {
        int flag;             /**< 1: supported, 0: not supported */
        u_long BUTTON_OK;     /**< Button code for OK/confirm */
        u_long BUTTON_CANCEL; /**< Button code for cancel */
    } type4;                  /**< NeGcon */
} sMcGuiController;

/**
 * @brief Cursor data structure
 *
 * Sets the color and shape of the menu cursor.
 */
typedef struct sMcGuiCursor {
    char type; /**< Cursor shape (currently not supported) */
    u_char r;  /**< Red color code (0-255) */
    u_char g;  /**< Green color code (0-255) */
    u_char b;  /**< Blue color code (0-255) */
} sMcGuiCursor;

/**
 * @brief BGM and sound effects data structure
 *
 * Contains information about BGM and sound effects for the Memory Card screen.
 */
typedef struct sMcGuiSnd {
    int MVOL;        /**< Main volume (0-127) */
    int isReverb;    /**< 0: reverb OFF, 1: reverb ON */
    int reverbType;  /**< Reverb type */
    int reverbDepth; /**< Reverb depth (0-127) */
    struct {
        int isbgm;   /**< 0: no BGM, 1: BGM */
        u_long* seq; /**< Header address of SEQ data */
        u_long* vh;  /**< Header address of VH data for SEQ */
        u_long* vb;  /**< Header address of VB data for SEQ */
        int SVOL;    /**< SEQ volume (0-127) */
    } bgm;           /**< BGM configuration */
    struct {
        int isse;        /**< 0: no sound effects, 1: sound effects */
        u_long* vh;      /**< Header address of VH data for SE */
        u_long* vb;      /**< Header address of VB data for SE */
        int vol;         /**< Sound effects volume */
        int prog;        /**< Sound effects program number */
        int TONE_OK;     /**< Tone number of confirmation/execution tone */
        int TONE_CANCEL; /**< Tone number of CANCEL tone */
        int TONE_CURSOR; /**< Tone number during cursor operation */
        int TONE_ERROR;  /**< Tone number of error tone */
    } se;                /**< Sound effects configuration */
} sMcGuiSnd;

/**
 * @brief Texture data structure
 *
 * Specifies the texture data used internally by the module.
 * For format information, refer to the Run-time Library Overview.
 */
typedef struct sMcGuiTexture {
    u_long* addr; /**< Header address of TIM data */
} sMcGuiTexture;

/**
 * @brief Memory Card GUI module main structure
 *
 * This is the main structure used by the Memory Card GUI module.
 * All settings and data are configured through this structure.
 */
typedef struct McGuiEnv {
    sMcGuiCards cards;           /**< Memory Card data structure */
    sMcGuiBg bg;                 /**< BG data structure */
    sMcGuiController controller; /**< Controller-related data structure */
    sMcGuiSnd sound;             /**< BGM and sound effect data structure */
    sMcGuiTexture texture;       /**< Texture data structure */
    sMcGuiCursor cursor;         /**< Cursor data structure */
} McGuiEnv;

/**
 * @brief Load game data
 *
 * Invokes the load operation of the Memory Card screen. Terminates when the
 * load has completed or when the cancel button is clicked on the Slot
 * Selection screen.
 *
 * @param env Memory Card GUI module structure
 * @return 1: load operation completed successfully,
 *         0: cancel button used to terminate,
 *         -1: invalid value in McGuiEnv structure
 */
int McGuiLoad(McGuiEnv* env);

/**
 * @brief Save game data
 *
 * Invokes the save operation of the Memory Card screen. Terminates when the
 * save is completed or when the cancel button is clicked on the Slot Selection
 * screen.
 *
 * @param env Memory Card GUI module structure
 * @return 1: save operation completed successfully,
 *         0: cancel button used to terminate,
 *         -1: invalid value in McGuiEnv structure
 */
int McGuiSave(McGuiEnv* env);

/**
 * @brief Set English mode
 *
 * Switches the messages used on the Memory Card screen to English. For display
 * in Japanese, this function should not be executed.
 *
 * @param env Memory Card GUI module structure
 * @param mode Language mode (always 1 for English)
 * @return Status code
 */
int McGuiSetExternalFont(McGuiEnv* env, int mode);

#endif /* MCGUI_H */
