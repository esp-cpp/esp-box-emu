#include <string.h>

#include "emuapi.h"
#include "iopins.h" 

extern "C" {
#include "shared.h"
}

static int rom_offset = 0;
static unsigned char * MemPool;

extern "C" uint8 read_rom(int address) {
  return (MemPool[address+rom_offset]); 
}

extern "C" void  write_rom(int address, uint8 val)  {
  printf("write_rom %d %d\n", address, val);
  // MemPool[address]=val;
}

extern "C" void sms_Init(void)
{
  printf("Allocating MEM\n");
  // MemPool = (unsigned char*)emu_Malloc(2*1024*1024);
  mem_init();    
  printf("Allocating MEM done\n");
}

extern "C" void sms_DeInit(void)
{
  printf("Deallocating MEM\n");
  // emu_Free(MemPool);
  mem_deinit();
  system_shutdown();
  system_deinit();
  printf("Deallocating MEM done\n");
}

extern "C" void sms_Input(int click) {
  // TODO: update the input structures with current input data from the box-hal
  // ik  = emu_GetPad();
  // ihk = emu_ReadI2CKeyboard();
}


void sms_low_level_start(uint8_t *romdata, size_t rom_data_size)
{
  printf("load and init\n");

  int size = rom_data_size;
  MemPool = (unsigned char*)romdata;
  if((size / 512) & 1)
  {
    size -= 512;
    rom_offset += 512;
  }
  cart.pages = (size / 0x4000);

  system_init(22050); // sound rate
  // system_init(0); // no sound
  printf("init done\n");
}


extern "C" void sms_Start(uint8_t *romdata, size_t rom_data_size)
{
  printf("Master system\n");
  cart.type = TYPE_SMS;
  sms_low_level_start(romdata, rom_data_size);
}

extern "C" void gg_Start(uint8_t *romdata, size_t rom_data_size)
{
  printf("Game Gear\n");
  cart.type = TYPE_GG;
  sms_low_level_start(romdata, rom_data_size);
}

extern "C" void sms_Step(void)
{
  input.pad[0]=0;

  // int k  = ik;
  // int hk = ihk;
  // if (iusbhk) hk = iusbhk;

  // if (( k & MASK_JOY1_RIGHT) || ( k & MASK_JOY2_RIGHT)) {
  //   input.pad[0] |= INPUT_RIGHT;
  // }
  // if (( k & MASK_JOY1_LEFT) || ( k & MASK_JOY2_LEFT)) {
  //   input.pad[0] |= INPUT_LEFT;
  // }
  // if (( k & MASK_JOY1_UP) || ( k & MASK_JOY2_UP)) {
  //   input.pad[0] |= INPUT_UP;
  // }
  // if (( k & MASK_JOY1_DOWN) || ( k & MASK_JOY2_DOWN)) {
  //   input.pad[0] |= INPUT_DOWN;
  // }
  // if (( k & MASK_JOY1_BTN) || ( k & MASK_JOY2_BTN))  {
  //   input.pad[0] |= INPUT_BUTTON1;
  // }

  // if ( (k & MASK_KEY_USER2) || (hk == 'q') )  {
  //   input.pad[0] |= INPUT_BUTTON2;
  // }

  // if ( (k & MASK_KEY_USER1) || (hk == 'w') )  input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;
  // if (hk == 'r')  input.system |= INPUT_HARD_RESET;
  // //if (hk == 'e')  input.system |= (IS_GG) ? INPUT_HARD_RESET : INPUT_SOFT_RESET;

  // prevhk = hk;

  sms_frame(0); 
  //emu_printi(emu_FrameSkip());

  emu_DrawVsync();    
}

void SND_Process(void *stream, int len) {
#ifdef HAS_SND
  audio_play_sample((int16*)stream, 0, len);
#endif
} 
