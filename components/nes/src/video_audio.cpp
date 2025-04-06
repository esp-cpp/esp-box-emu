#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <sdkconfig.h>

#include <math.h>
#include <string.h>
#include <stdint.h>

extern "C" {
#include <noftypes.h>
#include <bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <log.h>
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
#include <osd.h>
#include <nes/nesstate.h>
}

#include "box-emu.hpp"

#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

//Seemingly, this will be called only once. Should call func with a freq of frequency,
extern "C" int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
    return 0;
}

/*
** Audio
*/
static void (*audio_callback)(void *buffer, int length) = NULL;

extern "C" void do_audio_frame() {
    if (audio_callback == NULL) return;
    static int num_channels = 2;
    static int num_samples = BoxEmu::get().audio_sample_rate() * num_channels / NES_REFRESH_RATE;
    static int num_bytes = num_samples * sizeof(int16_t);
    static int16_t *audio_frame = (int16_t*)heap_caps_malloc(num_bytes, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    audio_callback(audio_frame, num_samples);
    BoxEmu::get().play_audio((uint8_t*)audio_frame, num_bytes);
}

extern "C" void osd_setsound(void (*playfunc)(void *buffer, int length))
{
    //Indicates we should call playfunc() to get more data.
    audio_callback = playfunc;
}

static void osd_stopsound(void)
{
   audio_callback = NULL;
}


static int osd_init_sound(void) {
	audio_callback = NULL;
	return 0;
}

extern "C" void osd_getsoundinfo(sndinfo_t *info)
{
   info->sample_rate = BoxEmu::get().audio_sample_rate();
   info->bps = 16;
}

/*
** Video
*/

static int init(int width, int height);
static void shutdown(void);
static int set_mode(int width, int height);
static void set_palette(rgb_t *pal);
static void clear(uint8 color);
static bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);
static void custom_blit(const bitmap_t *bmp, int num_dirties, rect_t *dirty_rects);

viddriver_t sdlDriver =
{
   "Simple DirectMedia Layer",         /* name */
   init,          /* init */
   shutdown,      /* shutdown */
   set_mode,      /* set_mode */
   set_palette,   /* set_palette */
   clear,         /* clear */
   lock_write,    /* lock_write */
   free_write,    /* free_write */
   (void (*)(bitmap_t *, int,  rect_t *))custom_blit,   /* custom_blit */
   false          /* invalidate flag */
};

bitmap_t *myBitmap;

void osd_getvideoinfo(vidinfo_t *info)
{
   info->default_width = DEFAULT_WIDTH;
   info->default_height = DEFAULT_HEIGHT;
   info->driver = &sdlDriver;
}

/* flip between full screen and windowed */
void osd_togglefullscreen(int code)
{
}

/* initialise video */
static int init(int width, int height)
{
	return 0;
}

static void shutdown(void)
{
}

/* set a video mode */
static int set_mode(int width, int height)
{
   return 0;
}

uint16 *myPalette = nullptr;

/* copy nes palette over to hardware */
static void set_palette(rgb_t *pal)
{
	uint16 c;

   int i;

   myPalette = (uint16_t*)_my_malloc(256 * sizeof(uint16));

   printf("set palette!\n");
   for (i = 0; i < 256; i++)
   {
      c = make_color(pal[i].r, pal[i].g, pal[i].b);
      myPalette[i]= c;
   }

}

uint16_t* get_nes_palette() {
    return (uint16_t*)myPalette;
}

/* clear all frames to a particular color */
static void clear(uint8 color)
{
}

/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{
//   SDL_LockSurface(mySurface);
   myBitmap = bmp_createhw((uint8*)BoxEmu::get().frame_buffer1(), DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_WIDTH*2);
   // make sure they don't try to delete the frame buffer lol
   myBitmap->hardware = true;
   return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
   bmp_destroy(&myBitmap);
}

static void custom_blit(const bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {
    uint8_t *lcdfb = BoxEmu::get().frame_buffer0();
    if (bmp->line[0] != NULL)
    {
        memcpy(lcdfb, bmp->line[0], 256 * 224);

        void* arg = (void*)lcdfb;
        BoxEmu::get().push_frame(arg);
    }
}

/*
** Input
*/

static int ConvertJoystickInput()
{
	int result = 0;

    auto state = BoxEmu::get().gamepad_state();

	if (!state.a)
		result |= (1<<13);
	if (!state.b)
		result |= (1 << 14);
	if (!state.select)
		result |= (1 << 0);
	if (!state.start)
		result |= (1 << 3);
	if (!state.right)
        result |= (1 << 5);
	if (!state.left)
        result |= (1 << 7);
	if (!state.up)
        result |= (1 << 4);
	if (!state.down)
        result |= (1 << 6);

	return result;
}


extern nes_t* console_nes;
extern nes6502_context cpu;

extern "C" void osd_getinput(void)
{
	static const int ev[16]={
			event_joypad1_select,0,0,event_joypad1_start,event_joypad1_up,event_joypad1_right,event_joypad1_down,event_joypad1_left,
			0,0,0,0,event_soft_reset,event_joypad1_a,event_joypad1_b,event_hard_reset
		};
	static int oldb=0xffff;
	int b=ConvertJoystickInput();
	int chg=b^oldb;
	int x;
	oldb=b;
	event_t evh;
	for (x=0; x<16; x++) {
		if (chg&1) {
			evh=event_get(ev[x]);
			if (evh) evh((b&1)?INP_STATE_BREAK:INP_STATE_MAKE);
		}
		chg>>=1;
		b>>=1;
	}
}

// cppcheck-suppress constParameterPointer
extern "C" void osd_getmouse(int *x, int *y, int *button)
{
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
extern "C" void osd_shutdown()
{
	osd_stopsound();
}

static int logprint(const char *string)
{
   return printf("%s", string);
}

/*
** Startup
*/
// Boot state overrides
bool forceConsoleReset = false;

int osd_init()
{
	log_chain_logfunc(logprint);
	osd_init_sound();
	return 0;
}
