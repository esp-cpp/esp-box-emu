#ifndef DRAW_H
#define DRAW_H

// ===== GPU backend =====
// as long as the platform implements these API calls, psy-z will work natively
//
// these API are mostly a personal reinterpretation of the following docs:
// https://psx-spx.consoledev.net/graphicsprocessingunitgpu

typedef struct {
    u_long tex_x : 4;
    u_long tex_y : 1;
    u_long semi_transparency : 2;
    u_long bpp : 2;    // this might be wrong? flipped with semi_transparency?
    u_long dither : 1; // 0: disable dither on 16-bit, 1: it is permitted
    u_long dfe : 1;    // 0: drawing to display area is blocked, 1: is permitted
    u_long tex_y_extra_vram : 1;
    u_long tex_flip_x : 1;
    u_long tex_flip_y : 1;
} ParamDrawTexpageMode;

// https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp108h-display-mode
typedef struct {
    unsigned int horizontal_resolution : 2;     // 0=256, 1=320, 2=512, 3=640
    unsigned int vertical_resolution : 1;       // 0=240p, 1=480i
    unsigned int pal : 1;                       // 0=NTSC/60Hz, 1=PAL/50Hz
    unsigned int rgb24 : 1;                     // 0=15bit, 1=24bit
    unsigned int interlaced : 1;                // 0=Off, 1=On
    unsigned int horizontal_resolution_368 : 1; // 0=256/320/512/640, 1=368
    unsigned int reversed : 1;                  // 0=Off, 1=On
    unsigned int : 24;
} DisplayMode;

// Reset everything related to the drawing logic.
// Implements ResetGraph.
// https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp100h-reset-gpu
void Draw_Reset();

// Allow to display the rendered content.
// On real hardware setting this to off will display a black screen.
// Implements SetDispMask.
// https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp103h-display-enable
void Draw_DisplayEnable(unsigned int on);

// Defines which location of the VRAM should be displayed. This essentially
// chooses which framebuffer is going to be used.
// Implements PutDispEnv.
// https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp105h-start-of-display-area-in-vram
void Draw_DisplayArea(unsigned int x, unsigned int y);

// Set range of horizontal lines to display.
// Implements PutDispEnv.
// https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp106h-horizontal-display-range-on-screen
void Draw_DisplayHorizontalRange(unsigned int start, unsigned int end);

// Set range of vertical lines to display.
// The range changes between PAL and NTSC.
// Implements PutDispEnv, SetVideoMode.
// https://psx-spx.consoledev.net/graphicsprocessingunitgpu/#gp107h-vertical-display-range-on-screen
void Draw_DisplayVerticalRange(unsigned int start, int unsigned end);

// Set various properties such as refresh rate, interlaced mode, 24bpp mode, etc
// Implements PutDispEnv
void Draw_SetDisplayMode(DisplayMode* mode);

void Draw_SetTexpageMode(ParamDrawTexpageMode* p);
void Draw_SetTextureWindow(unsigned int mask_x, unsigned int mask_y,
                           unsigned int off_x, unsigned int off_y);
void Draw_SetAreaStart(int x, int y);
void Draw_SetAreaEnd(int x, int y);
void Draw_SetOffset(int x, int y);
void Draw_SetMask(int bit0, int bit1);
void Draw_ClearImage(RECT* rect, u_char r, u_char g, u_char b);
void Draw_LoadImage(RECT* rect, u_long* p);
void Draw_StoreImage(RECT* rect, u_long* p);
void Draw_MoveImage(RECT* rect, unsigned int x, unsigned int y);
void Draw_PutDispEnv(DISPENV* disp);
void Draw_ResetBuffer(void);
void Draw_FlushBuffer(void);
int Draw_PushPrim(u_long* packets, int max_len);
int Draw_ExequeSync();

#endif
