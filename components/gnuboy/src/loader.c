#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "nvs_flash.h"
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


// static char *romfile=NULL;
// static char *sramfile=NULL;
static char *rtcfile=NULL;
// static char *saveprefix=NULL;

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
static byte *_data_ptr = NULL;

int rom_load(uint8_t *rom_data, size_t rom_data_size)
{
	/*byte c, *data, *header;
	int len = 0, rlen;
	BankCache[0] = 1;
	*/
	byte c, *data, *header;
	int len = 0, rlen;
	data = heap_caps_malloc(0x400000, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    _data_ptr = data;

    memcpy(data, rom_data, rom_data_size);
	// data = rom_data;
	printf("Initialized. ROM@%p\n", data);
	header = data;

	memcpy(rom.name, header+0x0134, 16);
	//if (rom.name[14] & 0x80) rom.name[14] = 0;
	//if (rom.name[15] & 0x80) rom.name[15] = 0;
	rom.name[16] = 0;
	printf("loader: rom.name='%s'\n", rom.name);

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
	//rom.bank[0] = data;
	rom.bank = data;
	rom.length = rlen;

	// SRAM
	ram.sram_dirty = 1;
	ram.sbank = malloc(sram_length);
	if (!ram.sbank)
	{
		// not enough free RAM,
		// check if PSRAM has free space
		if (rlen <= (0x100000 * 3) &&
			sram_length <= 0x100000)
		{
			ram.sbank = data + (0x100000 * 3);
			printf("SRAM using PSRAM.\n");
		}
		else
		{
			printf("No free spece for SRAM.\n");
			abort();
		}
	}


	initmem(ram.sbank, 8192 * mbc.ramsize);
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

	/*
	const esp_partition_t* part;
	spi_flash_mmap_handle_t hrom;
	esp_err_t err;

	part=esp_partition_find_first(0x40, 2, NULL);
	if (part==0)
	{
		printf("esp_partition_find_first (save) failed.\n");
		//abort();
	}
	else
	{
		err = esp_partition_read(part, 0, ram.sbank, mbc.ramsize * 8192);
		if (err != ESP_OK)
		{
			printf("esp_partition_read failed. (%d)\n", err);
		}
		else
		{
			printf("sram_load: sram load OK.\n");
			ram.sram_dirty = 0;
		}
	}*/

	return 0;
}


int sram_save()
{
	/* If we crash before we ever loaded sram, DO NOT SAVE! */
	if (!mbc.batt || !ram.loaded || !mbc.ramsize)
		return -1;

	/*const esp_partition_t* part;
	spi_flash_mmap_handle_t hrom;
	esp_err_t err;

	part=esp_partition_find_first(0x40, 2, NULL);
	if (part==0)
	{
		printf("esp_partition_find_first (save) failed.\n");
		//abort();
	}
	else
	{
		err = esp_partition_erase_range(part, 0, mbc.ramsize * 8192);
		if (err!=ESP_OK)
		{
			printf("esp_partition_erase_range failed. (%d)\n", err);
			abort();
		}

		err = esp_partition_write(part, 0, ram.sbank, mbc.ramsize * 8192);
		if (err != ESP_OK)
		{
			printf("esp_partition_write failed. (%d)\n", err);
		}
		else
		{
				printf("sram_load: sram save OK.\n");
		}
	}*/

	return 0;
}


void state_save(int n)
{
    /*
	FILE *f;
	char *name;

	if (n < 0) n = saveslot;
	if (n < 0) n = 0;
	name = malloc(strlen(saveprefix) + 5);
	sprintf(name, "%s.%03d", saveprefix, n);

	if ((f = fopen(name, "wb")))
	{
		savestate(f);
		fclose(f);
	}
	free(name);
	*/
}


void state_load(int n)
{
    /*
	FILE *f;
	char *name;

	if (n < 0) n = saveslot;
	if (n < 0) n = 0;
	name = malloc(strlen(saveprefix) + 5);
	sprintf(name, "%s.%03d", saveprefix, n);

	if ((f = fopen(name, "rb")))
	{
		loadstate(f);
		fclose(f);
		vram_dirty();
		pal_dirty();
		sound_dirty();
		mem_updatemap();
	}
	free(name);
    */
}

void rtc_save()
{
	FILE *f;
	if (!rtc.batt) return;
	if (!(f = fopen(rtcfile, "wb"))) return;
	rtc_save_internal(f);
	fclose(f);
}

void rtc_load()
{
	//FILE *f;
	if (!rtc.batt) return;
	//if (!(f = fopen(rtcfile, "r"))) return;
	//rtc_load_internal(f);
	//fclose(f);
}


void loader_unload()
{
    printf("freeing data\n");
    if (_data_ptr != NULL)
        free(_data_ptr);
    printf("data freed!\n");
    /*
	sram_save();
	if (romfile) free(romfile);
	if (sramfile) free(sramfile);
	if (saveprefix) free(saveprefix);
	if (rom.bank) free(rom.bank);
	if (ram.sbank) free(ram.sbank);
	romfile = sramfile = saveprefix = 0;
	rom.bank = 0;
	ram.sbank = 0;
	mbc.type = mbc.romsize = mbc.ramsize = mbc.batt = 0;
    */
}

/* basename/dirname like function */
static char *base(char *s)
{
	char *p;
	p = (char *) strrchr((unsigned char)s, DIRSEP_CHAR);
	if (p) return p+1;
	return s;
}

static char *ldup(char *s)
{
	int i;
	char *n, *p;
	p = n = malloc(strlen(s));
	for (i = 0; s[i]; i++) if (isalnum((unsigned char)s[i])) *(p++) = tolower((unsigned char)s[i]);
	*p = 0;
	return n;
}

static void cleanup()
{
	sram_save();
	rtc_save();
	/* IDEA - if error, write emergency savestate..? */
}

void loader_init(uint8_t *romptr, size_t rom_size)
{
	rom_load(romptr, rom_size);
	rtc_load();
}

#ifndef GNUBOY_NO_MINIZIP
/*
** Simplistic zip support, only loads the first file in a zip file
** with no check as to the type (or filename/extension).
*/

/*
**  returns 1 if a filename is a zip file
*/
static int check_zip(char *filename)
{
    char buf[2];
    FILE *fd = NULL;
    fd = fopen(filename, "rb");
    if(!fd) return (0);
    fread(buf, 2, 1, fd);
    fclose(fd);
    if(memcmp(buf, "PK", 2) == 0) return (1);
    return (0);
}

static byte *loadzipfile(char *archive, int *filesize)
{
    char name[256];
    unsigned char *buffer=NULL;
    int zerror = UNZ_OK;
    unzFile zhandle;
    unz_file_info zinfo;
    char tmp_header[0x200];
    unsigned char rom_found=0;

    zhandle = unzOpen(archive);
    if(!zhandle) return (NULL);

#ifdef IS_LITTLE_ENDIAN
#define GAMEBOY_HEADER_MAGIC 0x6666EDCE
#else /* BIG ENDIAN */
#define GAMEBOY_HEADER_MAGIC 0xCEED6666
#endif /* IS_LITTLE_ENDIAN */

    /* Find first gameboy rom, do not use file extension, look for magic bytes/fingerprint */
    /* Seek to first file in archive */
    zerror = unzGoToFirstFile(zhandle);
    if(zerror != UNZ_OK)
    {
        unzClose(zhandle);
        return (NULL);
    }

    do
    {
        unzOpenCurrentFile(zhandle);
        unzReadCurrentFile(zhandle, tmp_header, sizeof(tmp_header));
        unzCloseCurrentFile(zhandle);
        if ((*((unsigned long *)(tmp_header + 0x104))) == GAMEBOY_HEADER_MAGIC)
        {
            /* Gameboy Rom found! */
            rom_found++;
            break;
        }
    } while (unzGoToNextFile(zhandle) != UNZ_END_OF_LIST_OF_FILE);

    if (rom_found == 0)
        return (NULL);

    /* Get information about the file */
    unzGetCurrentFileInfo(zhandle, &zinfo, &name[0], 0xff, NULL, 0, NULL, 0);
    *filesize = zinfo.uncompressed_size;

    /* Error: file size is zero */
    if(*filesize <= 0)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Open current file */
    zerror = unzOpenCurrentFile(zhandle);
    if(zerror != UNZ_OK)
    {
        unzClose(zhandle);
        return (NULL);
    }

    /* Allocate buffer and read in file */
    buffer = malloc(*filesize);
    if(!buffer) return (NULL);
    zerror = unzReadCurrentFile(zhandle, buffer, *filesize);

    /* Internal error: free buffer and close file */
    if(zerror < 0 || zerror != *filesize)
    {
        free(buffer);
        buffer = NULL;
        unzCloseCurrentFile(zhandle);
        unzClose(zhandle);
        return (NULL);
    }

    /* Close current file and archive file */
    unzCloseCurrentFile(zhandle);
    unzClose(zhandle);

    return (buffer);
}
#endif /* GNUBOY_USE_MINIZIP */
