// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>

//Nes stuff wants to define this as well...
#undef false
#undef true
#undef bool

#include <math.h>
#include <string.h>
#include <noftypes.h>
#include <bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <log.h>
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
#include <osd.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "../nofrendo/nes/nesstate.h"

// from box-emu-hal
#include "spi_lcd.h"
#include "i2s_audio.h"
#include "input.h"

#define  DEFAULT_FRAGSIZE    4096

#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

//Seemingly, this will be called only once. Should call func with a freq of frequency,
int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
    return 0;
}

/*
** Audio
*/
static void (*audio_callback)(void *buffer, int length) = NULL;
static int16_t *audio_frame;

void do_audio_frame() {
    int remaining = AUDIO_SAMPLE_RATE / NES_REFRESH_RATE;
    while(remaining) {
        int n=DEFAULT_FRAGSIZE;
        if (n>remaining) n=remaining;

        //get more data
        audio_callback(audio_frame, n);

        //16 bit mono -> 32-bit (16 bit r+l)
        for (int i=n-1; i>=0; i--) {
            int sample = (int)audio_frame[i];
            audio_frame[i*2+1] = (short)sample;
            audio_frame[i*2] = (short)sample;
        }
        audio_play_frame(audio_frame, 2*n);

        remaining -= n;
    }
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
    //Indicates we should call playfunc() to get more data.
    audio_callback = playfunc;
}

static void osd_stopsound(void)
{
   audio_callback = NULL;
}


static int osd_init_sound(void) {
	audio_frame=get_audio_buffer();
    audio_init();
	audio_callback = NULL;
	return 0;
}

void osd_getsoundinfo(sndinfo_t *info)
{
   info->sample_rate = AUDIO_SAMPLE_RATE;
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
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects);

QueueHandle_t vidQueue;

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
   custom_blit,   /* custom_blit */
   false          /* invalidate flag */
};

// NES
#define NES_GAME_WIDTH (256)
#define NES_GAME_HEIGHT (224) /* NES_VISIBLE_HEIGHT */

void ili9341_write_frame_nes(const uint8_t* buffer, uint16_t* myPalette, uint8_t scale) {
    short x, y;
    if (buffer == NULL) {
        // clear the buffer, clear the screen
        lcd_write_frame(0, 0, NES_GAME_WIDTH-1, NES_GAME_HEIGHT-1, NULL);
    } else {
        uint8_t* framePtr = buffer;
        static int buffer_index = 0;
        static const int LINE_COUNT = 50;
        for (y = 0; y < NES_GAME_HEIGHT; y+= LINE_COUNT) {
            uint16_t* line_buffer = buffer_index ? (uint16_t*)get_vram1() : (uint16_t*)get_vram0();
            buffer_index = buffer_index ? 0 : 1;
            int num_lines_written = 0;
            for (int i=0; i<LINE_COUNT; i++) {
                int src_y = y+i;
                if (src_y >= NES_GAME_HEIGHT) break;
                for (x=0; x<NES_GAME_WIDTH; ++x) {
                    int src_index = (src_y)*NES_GAME_WIDTH + x;
                    int dst_index = i*NES_GAME_WIDTH + x;
                    line_buffer[dst_index] = (uint16_t)myPalette[framePtr[src_index]];
                }
                num_lines_written++;
            }
            lcd_write_frame(0, y, NES_GAME_WIDTH, num_lines_written, (uint8_t*)&line_buffer[0]);
        }
    }
}

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

uint16 myPalette[256];

/* copy nes palette over to hardware */
static void set_palette(rgb_t *pal)
{
	uint16 c;

   int i;

   printf("set palette!\n");
   for (i = 0; i < 256; i++)
   {
      c = make_color(pal[i].r, pal[i].g, pal[i].b);
      myPalette[i]= c;
   }

}

/* clear all frames to a particular color */
static void clear(uint8 color)
{
}

/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{
//   SDL_LockSurface(mySurface);
   myBitmap = bmp_createhw((uint8*)get_frame_buffer1(), DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_WIDTH*2);
   return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
   bmp_destroy(&myBitmap);
}

static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {
    uint8_t *lcdfb = get_frame_buffer0();
    if (bmp->line[0] != NULL)
    {
        memcpy(lcdfb, bmp->line[0], 256 * 224);

        void* arg = (void*)lcdfb;
    	xQueueSend(vidQueue, &arg, portMAX_DELAY);
    }
}

//This runs on core 1.
volatile bool exitVideoTaskFlag = false;
static void videoTask(void *arg) {
    uint8_t* bmp = NULL;

    while(1)
	{
		xQueuePeek(vidQueue, &bmp, portMAX_DELAY);

        if (bmp == 1) break;

        ili9341_write_frame_nes(bmp, myPalette, false);

		xQueueReceive(vidQueue, &bmp, portMAX_DELAY);
	}

    exitVideoTaskFlag = true;

    vTaskDelete(NULL);

    while(1){}
}


/*
** Input
*/

static void osd_initinput()
{
    init_input();
}


static void SaveState()
{
    printf("Saving state.\n");

    save_sram();

    printf("Saving state OK.\n");
}

static void PowerDown()
{
    uint16_t* param = 1;

    // Stop tasks
    printf("PowerDown: stopping tasks.\n");

    xQueueSend(vidQueue, &param, portMAX_DELAY);
    while (!exitVideoTaskFlag) { vTaskDelay(1); }

    // state
    printf("PowerDown: Saving state.\n");
    SaveState();

    /*
    // LCD
    printf("PowerDown: Powerdown LCD panel.\n");

    printf("PowerDown: Entering deep sleep.\n");

    // Should never reach here
    abort();
    */
}

static int ConvertJoystickInput()
{
	int result = 0;

    static struct InputState state;
    get_input_state(&state);

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

void osd_getinput(void)
{
	const int ev[16]={
			event_joypad1_select,0,0,event_joypad1_start,event_joypad1_up,event_joypad1_right,event_joypad1_down,event_joypad1_left,
			0,0,0,0,event_soft_reset,event_joypad1_a,event_joypad1_b,event_hard_reset
		};
	static int oldb=0xffff;
    if (user_quit()) {
        nes_poweroff();
    }
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

static void osd_freeinput(void)
{
}

void osd_getmouse(int *x, int *y, int *button)
{
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
void osd_shutdown()
{
	osd_stopsound();
	osd_freeinput();
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

	if (osd_init_sound())
    {
        abort();
    }

	vidQueue=xQueueCreate(1, sizeof(bitmap_t *));
	xTaskCreatePinnedToCore(&videoTask, "videoTask", 6*1024, NULL, 20, NULL, 1);

    osd_initinput();

	return 0;
}
