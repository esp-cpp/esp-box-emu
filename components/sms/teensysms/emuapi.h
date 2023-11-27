#pragma once

#define PALETTE_SIZE         32
#define VID_FRAME_SKIP       0x0

#define ACTION_NONE          0
#define ACTION_RUN1          1
#define ACTION_RUN2          2

#define FORCE_VGATIMERVSYNC  1
#define SUPPORT_HIRES        1

#define MASK_JOY2_RIGHT 0x0001
#define MASK_JOY2_LEFT  0x0002
#define MASK_JOY2_UP    0x0004
#define MASK_JOY2_DOWN  0x0008
#define MASK_JOY2_BTN   0x0010
#define MASK_KEY_USER1  0x0020
#define MASK_KEY_USER2  0x0040
#define MASK_KEY_USER3  0x0080
#define MASK_JOY1_RIGHT 0x0100
#define MASK_JOY1_LEFT  0x0200
#define MASK_JOY1_UP    0x0400
#define MASK_JOY1_DOWN  0x0800
#define MASK_JOY1_BTN   0x1000
#define MASK_KEY_USER4  0x2000

#define RGBVAL16(r,g,b)  ( (((r>>3)&0x1f)<<11) | (((g>>2)&0x3f)<<5) | (((b>>3)&0x1f)<<0) )

#ifdef __cplusplus  
extern "C" {
void sms_emu_init();
void emu_start(int vblms, void * callback, int forcetimervsync=0);
#endif
void * emu_Malloc(unsigned int size);
void * emu_MallocI(unsigned int size);
void emu_Free(void * pt);
void * emu_SMalloc(unsigned int size);
void emu_SFree(void * pt);

int emu_FileOpen(const char * filepath, const char * mode);
int emu_FileRead(void * buf, int size, int handler);
int emu_FileWrite(void * buf, int size, int handler);
int emu_FileGetc(int handler);
int emu_FileSeek(int handler, int seek, int origin);
int emu_FileTell(int handler);
void emu_FileClose(int handler);

unsigned int emu_FileSize(const char * filepath);
unsigned int emu_LoadFile(const char * filepath, void * buf, int size);
unsigned int emu_LoadFileSeek(const char * filepath, void * buf, int size, int seek);

void emu_SetPaletteEntry(unsigned char r, unsigned char g, unsigned char b, int index);
void emu_DrawLinePal16(unsigned char * VBuf, int width, int height, int line);
void emu_DrawLine16(unsigned short * VBuf, int width, int height, int line);
void emu_DrawScreenPal16(unsigned char * VBuf, int width, int height, int stride);
void emu_DrawLine8(unsigned char * VBuf, int width, int height, int line);
void emu_DrawVsync(void);
int emu_FrameSkip(void);
int emu_IsVga(void);
int emu_IsVgaHires(void);

int menuActive(void);
char * menuSelection(void);
char * menuSecondSelection(void);
void toggleMenu(int on);
int  handleMenu(unsigned short bClick);

int handleOSKB(void);
void toggleOSKB(int forceon);

void emu_InitJoysticks(void);
int emu_SwapJoysticks(int statusOnly);
unsigned short emu_DebounceLocalKeys(void);
int emu_ReadKeys(void);
int emu_GetPad(void);
int emu_GetMouse(int *x, int *y, int *buts);
int emu_MouseDetected(void);
int emu_GetJoystick(void);
int emu_KeyboardDetected(void);
int emu_ReadAnalogJoyX(int min, int max);
int emu_ReadAnalogJoyY(int min, int max);
int emu_ReadI2CKeyboard(void);
unsigned char emu_ReadI2CKeyboard2(int row);
void emu_KeyboardOnUp(int keymodifer, int key);
void emu_KeyboardOnDown(int keymodifer, int key);
void emu_MidiOnDataReceived(unsigned char data);

void emu_sndPlaySound(int chan, int volume, int freq);
void emu_sndPlayBuzz(int size, int val);
void emu_sndInit();
void emu_resetus(void);
int emu_us(void);

int emu_setKeymap(int index);

#ifdef __cplusplus
}
#endif

