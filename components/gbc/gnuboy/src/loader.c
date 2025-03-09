#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "esp_partition.h"
#include "esp_system.h"
#include "esp_heap_caps.h"

#include "gnuboy/gnuboy.h"
#include "gnuboy/defs.h"
#include "gnuboy/regs.h"
#include "gnuboy/mem.h"
#include "gnuboy/hw.h"
#include "gnuboy/lcd.h"
#include "gnuboy/rtc.h"
#include "gnuboy/sound.h"

static int mbc_table[256] =
{
	0, 1, 1, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 3,
	3, 3, 3, 3, 0, 0, 0, 0, 0, 5, 5, 5, MBC_RUMBLE, MBC_RUMBLE, MBC_RUMBLE, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MBC_HUC3, MBC_HUC1
};

static int rtc_table[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0
};

static int batt_table[256] =
{
	0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0,
	1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0,
	0
};

static int romsize_table[256] =
{
	2, 4, 8, 16, 32, 64, 128, 256, 512,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 128, 128, 128
	/* 0, 0, 72, 80, 96  -- actual values but bad to use these! */
};

static int ramsize_table[256] =
{
	1, 1, 1, 4, 16,
	4 /* FIXME - what value should this be?! */
};


static char *rtcfile=NULL;

static char *savename=NULL;
static char *savedir=NULL;

static int saveslot=0;

static int forcebatt=0, nobatt=0;
static int forcedmg=0, gbamode=0;

static int memfill = 0, memrand = -1;

static void initmem(void *mem, int size)
{
	char *p = mem;
	//memset(p, 0xff /*memfill*/, size);
	if (memfill >= 0)
		memset(p, memfill, size);
}

static byte *inf_buf;
static int inf_pos, inf_len;
// static byte *_data_ptr = NULL;
static byte *sram_ptr = NULL;

int rom_load(uint8_t *rom_data, size_t rom_data_size)
{
	byte c, *data, *header;
	int len = 0, rlen;
    data = rom_data;
	printf("Initialized. ROM@%p\n", data);
	header = data;

	int tmp = *((int*)(header + 0x0144));
	c = (tmp >> 24) & 0xff;
	mbc.type = mbc_table[c];
	mbc.batt = (batt_table[c] && !nobatt) || forcebatt;
	rtc.batt = rtc_table[c];

	tmp = *((int*)(header + 0x0148));
	mbc.romsize = romsize_table[(tmp & 0xff)];
	mbc.ramsize = ramsize_table[((tmp >> 8) & 0xff)];

	if (!mbc.romsize) die("unknown ROM size %02X\n", header[0x0148]);
	if (!mbc.ramsize) die("unknown SRAM size %02X\n", header[0x0149]);

	const char* mbcName;
	switch (mbc.type)
	{
		case MBC_NONE:
			mbcName = "MBC_NONE";
			break;

		case MBC_MBC1:
			mbcName = "MBC_MBC1";
			break;

		case MBC_MBC2:
			mbcName = "MBC_MBC2";
			break;

		case MBC_MBC3:
			mbcName = "MBC_MBC3";
			break;

		case MBC_MBC5:
			mbcName = "MBC_MBC5";
			break;

		case MBC_RUMBLE:
			mbcName = "MBC_RUMBLE";
			break;

		case MBC_HUC1:
			mbcName = "MBC_HUC1";
			break;

		case MBC_HUC3:
			mbcName = "MBC_HUC3";
			break;

		default:
			mbcName = "(unknown)";
			break;
	}

	rlen = 16384 * mbc.romsize;
	int sram_length = 8192 * mbc.ramsize;
	printf("loader: mbc.type=%s, mbc.romsize=%d (%dK), mbc.ramsize=%d (%dK)\n", mbcName, mbc.romsize, rlen / 1024, mbc.ramsize, sram_length / 1024);

	// ROM
	rom.bank = (byte (*)[16384])data;
	rom.length = rlen;

	// SRAM
	ram.sram_dirty = 1;
	if (!sram_ptr) {
		sram_ptr = heap_caps_malloc(sram_length, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
	}
	ram.sbank = (byte (*)[8192])sram_ptr;

	initmem(ram.sbank, sram_length);
	initmem(ram.ibank, 4096 * 8);

	mbc.rombank = 1;
	mbc.rambank = 0;

	tmp = *((int*)(header + 0x0140));
	c = tmp >> 24;
	hw.cgb = ((c == 0x80) || (c == 0xc0)) && !forcedmg;
	hw.gba = (hw.cgb && gbamode);

	return 0;
}

int sram_load()
{
	if (!mbc.batt) return -1;
	/* Consider sram loaded at this point, even if file doesn't exist */
	ram.loaded = 1;
	return 0;
}


int sram_save()
{
	/* If we crash before we ever loaded sram, DO NOT SAVE! */
	if (!mbc.batt || !ram.loaded || !mbc.ramsize)
		return -1;
	return 0;
}


void state_save(int n)
{
}


void state_load(int n)
{
}

void rtc_save()
{
	/* FILE *f; */
	/* if (!rtc.batt) return; */
	/* if (!(f = fopen(rtcfile, "wb"))) return; */
	/* rtc_save_internal(f); */
	/* fclose(f); */
}

void rtc_load()
{
}


void loader_unload()
{
	sram_save();
	// rom.bank = NULL;
	// ram.sbank = NULL;
	mbc.type = mbc.romsize = mbc.ramsize = mbc.batt = 0;
}

static void cleanup()
{
	sram_save();
	rtc_save();
}

void loader_init(uint8_t *romptr, size_t rom_size)
{
	rom_load(romptr, rom_size);
	rtc_load();
}
