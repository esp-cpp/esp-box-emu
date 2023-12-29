#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <cstdio>

#include "emuapi.h"
#include "iopins.h"

// #include "bmpjoy.h"
// #include "bmpvbar.h"

// #include "esp_event.h"

extern "C" {

void sms_emu_init(void)
{
}


void emu_printf(char * text)
{
  printf("%s\n",text);
}


void emu_printi(int val)
{
  printf("%d\n",val);
}

void * emu_Malloc(unsigned int size)
{
  void * retval =  malloc(size);
  if (!retval) {
    printf("failled to allocate %d\n",size);
  }
  else {
    printf("could allocate %d\n",size); 
  }
  
  return retval;
}

void emu_Free(void * pt)
{
  free(pt);
  printf("freed memory\n");
}

static unsigned short palette16[PALETTE_SIZE];
static int fskip=0;

void emu_SetPaletteEntry(unsigned char r, unsigned char g, unsigned char b, int index)
{
  if (index<PALETTE_SIZE) {
    palette16[index] = RGBVAL16(r,g,b);
  }
}

void emu_DrawVsync(void)
{
  fskip += 1;
  fskip &= VID_FRAME_SKIP;
}

void emu_DrawLine(unsigned char * VBuf, int width, int height, int line) 
{
  if (fskip==0) {
    // TODO: use box-hal to send frame data to screen
    // tft.writeLine(width,height,line, VBuf, palette16);
  }
}  

void emu_DrawScreen(unsigned char * VBuf, int width, int height, int stride) 
{  
  if (fskip==0) {
    // TODO: use box-hal to send frame data to screen
    // tft.writeScreen(width,height-TFT_VBUFFER_YCROP,stride, VBuf+(TFT_VBUFFER_YCROP/2)*stride, palette16);
  }
}

int emu_FrameSkip(void)
{
  return fskip;
}

void * emu_LineBuffer(int line)
{
  // TODO: use box-hal to get line data from screen buffer
  // return (void*)tft.getLineBuffer(line);
  return nullptr;
}

}

#include "AudioPlaySystem.h"
extern AudioPlaySystem audio;

void emu_sndInit() {
}

void emu_sndPlaySound(int chan, int volume, int freq)
{
  if (chan < 6) {
    audio.sound(chan, freq, volume); 
  } 
}

void emu_sndPlayBuzz(int size, int val) {
  //mymixer.buzz(size,val);  
}
