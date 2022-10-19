//
// Created by Ethan Williams on 2019-01-29.
//

#ifndef NESEMU_PPU_H
#define NESEMU_PPU_H

#include <stdint.h>
#include "utils.h"
#include "PPUmemory.h"
#include "nes_clock.h"

class NesCpu;

#define POSTRENDER_SCANLINE 240
#define PRERENDER_SCANLINE 261
#define PIXELS_PER_LINE 341
#define BACKGROUND_PALETTE_ADDRESS 0x3F00
#define SPRITE_PALETTE_ADDRESS 0x3F10
#define SCREEN_X 256
#define SCREEN_Y 240

#define MAKE_ARGB(R,G,B) (R)|(G<<8)|(B<<16)

/* union regtype taken from https://bisqwit.iki.fi/jutut/kuvat/programming_examples/nesemu1/nesemu1.cc */

union PPUregtype // PPU register file
{
	uint32_t value;
	// Reg0 (write)                  // Reg1 (write)                  // Reg2 (read)
	RegBit<0,8,uint32_t> sysctrl;    RegBit< 8,8,uint32_t> dispctrl;  RegBit<16,8,uint32_t> status;
	RegBit<0,2,uint32_t> BaseNTA;    RegBit< 8,1,uint32_t> Grayscale; RegBit<21,1,uint32_t> SPoverflow;
	RegBit<2,1,uint32_t> Inc;        RegBit< 9,1,uint32_t> ShowBG8;   RegBit<22,1,uint32_t> SP0hit;
	RegBit<3,1,uint32_t> SPaddr;     RegBit<10,1,uint32_t> ShowSP8;   RegBit<23,1,uint32_t> VBlank;
	RegBit<4,1,uint32_t> BGaddr;     RegBit<11,1,uint32_t> ShowBG;    // Reg3 (write)
	RegBit<5,1,uint32_t> SPsize;     RegBit<12,1,uint32_t> ShowSP;    RegBit<24,8,uint32_t> OAMaddr;
	RegBit<6,1,uint32_t> SlaveFlag;  RegBit<11,2,uint32_t> ShowBGSP;  RegBit<24,2,uint32_t> OAMfield;
	RegBit<7,1,uint32_t> NMIenabled; RegBit<13,3,uint32_t> EmpRGB;    RegBit<26,6,uint32_t> OAMindex;
};

struct sprite_data_t {
	uint8_t bitmap_shift_reg[2];
	union {
		uint8_t data;
		RegBit<0,2,uint8_t> palette;
		RegBit<5,1,uint8_t> priority;
		RegBit<6,1,uint8_t> flip_H;
		RegBit<7,1,uint8_t> flip_V;
	} attribute;
	uint8_t x_position;
};


class PPU {
public:

        explicit PPU(Gamepak * gamepak);
        explicit PPU(Gamepak * gamepak, uint16_t *display_buffer);
        ~PPU();

        void power_up();
        void assign_cpu(NesCpu * cpu);

	uint8_t read_register(uint8_t address);
	void write_register(uint16_t address, uint8_t value);
	void OAM_DMA(uint8_t *CPU_memory);
	uint16_t * get_framebuffer();

	bool step();

private:
	NesCpu * cpu;
	PPUregtype reg;
	PPUmemory * memory;

	unsigned pixel;
	unsigned scanline;
	size_t frame_counter = 0;
	nes_ppu_clock_t cycle_counter;

	union {
		uint16_t data;
		RegBit<0,5,uint16_t> coarseX;
		RegBit<5,5,uint16_t> coarseY;
		RegBit<10,2,uint16_t> NTselect;
		RegBit<10,1,uint16_t> NTselectX;
		RegBit<11,1,uint16_t> NTselectY;
		RegBit<12,3,uint16_t> fineY;
	} vram_address, temp_vram_address;

	uint8_t x_fine_scroll;
	bool write_toggle;

	uint32_t bg_pixels[16];
	bool bg_pixel_valid[16];

	uint32_t fg_pixels[256];
	bool fg_pixel_valid[256];
	bool fg_pixel_infront[256];
	bool fg_pixel_sp0[256];

        uint32_t fg_sr_pixels[8];
        bool fg_sr_pixels_valid[8];

	uint16_t *frame_buffer;
	bool allocated_frame_buffer;

	OAM_entry * secondary_OAM[8];
	sprite_data_t sprite_data[8];
	inline constexpr static uint32_t ntsc_palette [64] = {
		MAKE_ARGB(0x75, 0x75, 0x75),
		MAKE_ARGB(0x27, 0x1B, 0x8F),
		MAKE_ARGB(0x00, 0x00, 0xAB),
		MAKE_ARGB(0x47, 0x00, 0x9F),
		MAKE_ARGB(0x8F, 0x00, 0x77),
		MAKE_ARGB(0xAB, 0x00, 0x13),
		MAKE_ARGB(0xA7, 0x00, 0x00),
		MAKE_ARGB(0x7F, 0x0B, 0x00),
		MAKE_ARGB(0x43, 0x2F, 0x00),
		MAKE_ARGB(0x00, 0x47, 0x00),
		MAKE_ARGB(0x00, 0x51, 0x00),
		MAKE_ARGB(0x00, 0x3F, 0x17),
		MAKE_ARGB(0x1B, 0x3F, 0x5F),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0xBC, 0xBC, 0xBC),
		MAKE_ARGB(0x00, 0x73, 0xEF),
		MAKE_ARGB(0x23, 0x3B, 0xEF),
		MAKE_ARGB(0x83, 0x00, 0xF3),
		MAKE_ARGB(0xBF, 0x00, 0xBF),
		MAKE_ARGB(0xE7, 0x00, 0x5B),
		MAKE_ARGB(0xDB, 0x2B, 0x00),
		MAKE_ARGB(0xCB, 0x4F, 0x0F),
		MAKE_ARGB(0x8B, 0x73, 0x00),
		MAKE_ARGB(0x00, 0x97, 0x00),
		MAKE_ARGB(0x00, 0xAB, 0x00),
		MAKE_ARGB(0x00, 0x93, 0x3B),
		MAKE_ARGB(0x00, 0x83, 0x8B),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0xFF, 0xFF, 0xFF),
		MAKE_ARGB(0x3F, 0xBF, 0xFF),
		MAKE_ARGB(0x5F, 0x97, 0xFF),
		MAKE_ARGB(0xA7, 0x8B, 0xFD),
		MAKE_ARGB(0xF7, 0x7B, 0xFF),
		MAKE_ARGB(0xFF, 0x77, 0xB7),
		MAKE_ARGB(0xFF, 0x77, 0x63),
		MAKE_ARGB(0xFF, 0x9B, 0x3B),
		MAKE_ARGB(0xF3, 0xBF, 0x3F),
		MAKE_ARGB(0x83, 0xD3, 0x13),
		MAKE_ARGB(0x4F, 0xDF, 0x4B),
		MAKE_ARGB(0x58, 0xF8, 0x98),
		MAKE_ARGB(0x00, 0xEB, 0xDB),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0xFF, 0xFF, 0xFF),
		MAKE_ARGB(0xAB, 0xE7, 0xFF),
		MAKE_ARGB(0xC7, 0xD7, 0xFF),
		MAKE_ARGB(0xD7, 0xCB, 0xFF),
		MAKE_ARGB(0xFF, 0xC7, 0xFF),
		MAKE_ARGB(0xFF, 0xC7, 0xDB),
		MAKE_ARGB(0xFF, 0xBF, 0xB3),
		MAKE_ARGB(0xFF, 0xDB, 0xAB),
		MAKE_ARGB(0xFF, 0xE7, 0xA3),
		MAKE_ARGB(0xE3, 0xFF, 0xA3),
		MAKE_ARGB(0xAB, 0xF3, 0xBF),
		MAKE_ARGB(0xB3, 0xFF, 0xCF),
		MAKE_ARGB(0x9F, 0xFF, 0xF3),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00),
		MAKE_ARGB(0x00, 0x00, 0x00)
	};

	void evaluate_sprites();
	void render_pixel();

	void load_bg_tile();
	void populateShiftRegister(uint8_t, uint16_t, bool, int);

	void increment_x();
	void increment_y();
	void h_to_v();
	void v_to_v();
};


#endif //NESEMU_PPU_H
