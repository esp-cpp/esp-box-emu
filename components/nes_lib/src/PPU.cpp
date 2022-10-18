//
// Created by Ethan Williams on 2019-01-29.
//
// Modified by William Emfinger on 2022-10-17
//

#include "spi_lcd.h"

#include "../include/PPU.h"
#include "../include/cpu.h"
#include "../include/PPUmemory.h"

#define NUM_ROWS_IN_FRAME_BUFFER 24
// SCREEN_X is 256, so this will end up being 8k
#define FRAME_BUFFER_SIZE (SCREEN_X*NUM_ROWS_IN_FRAME_BUFFER)

PPU::PPU(Gamepak * gamepak) {
	memory = new PPUmemory(gamepak);
	frame_buffer = new uint16_t [FRAME_BUFFER_SIZE];
}

void PPU::assign_cpu(NesCpu *cpu) {
	this->cpu = cpu;
}

PPU::~PPU() {
	delete frame_buffer;
	delete memory;
}

void PPU::power_up() {
	scanline = POSTRENDER_SCANLINE + 1;
	pixel = 0;
	reg.ShowBG = 1;
	cycle_counter = nes_ppu_clock_t(0);
}

uint8_t PPU::read_register(uint8_t address) {
	uint8_t result;
	switch (address) {
		case 2:
			write_toggle = 0;
			result = (uint8_t)reg.status;
			reg.VBlank = 0;
			return result;
		case 4:
			return memory->read_byte_OAM((uint8_t)reg.OAMaddr);
		case 7:
			result = memory->buffered_read_byte(vram_address.data);
			vram_address.data += reg.Inc ? 32 : 1;
			return result;
		default: return 0;
	}
}

void PPU::write_register(uint16_t address, uint8_t value) {
//	address %= 0x8;
	switch (address % 0x8) {
		case 0:
			reg.sysctrl = value;
			temp_vram_address.NTselect = reg.BaseNTA;
			break;
		case 1:
			reg.dispctrl = value; break;
		case 3:
			reg.OAMaddr = value; break;
		case 4:
			memory->write_byte_OAM((uint8_t)reg.OAMaddr, value);
			reg.OAMaddr++;
			break;
		case 5:
			if (write_toggle == 0) {
				temp_vram_address.coarseX = value >> 3;
				x_fine_scroll = (uint8_t)(value & 0b111);
			} else {
				temp_vram_address.fineY = value & 0b111;
				temp_vram_address.coarseY = value >> 3;
			}
			write_toggle = !write_toggle;
			break;
		case 6:
			 if (write_toggle == 0) {
			 	temp_vram_address.data = ((value & 0x003F) << 8) + (temp_vram_address.data & 0x00FF);
			 	temp_vram_address.data &= (uint16_t)0x7FFF;
			 } else {
			 	temp_vram_address.data = (temp_vram_address.data & 0xFF00) + value;
			 	vram_address.data = temp_vram_address.data;
			 }
			 write_toggle = !write_toggle;
			 break;
		case 7:
			memory->write_byte(vram_address.data, value);
			vram_address.data += reg.Inc ? 32 : 1;
			break;
		default: break;
	}
}

void PPU::OAM_DMA(uint8_t *CPU_memory) {
	uint8_t i = (uint8_t)reg.OAMaddr;
	do {
		memory->write_byte_OAM(i, *CPU_memory);
		CPU_memory++;
	} while (++i != reg.OAMaddr);
}

uint16_t* PPU::get_framebuffer() {
	return frame_buffer;
}

bool PPU::step() {
	/* Advance Cycle */
	pixel++;
	bool outputready = false;
	bool isOdd = ((frame_counter & 0x1) != 0);
	if (scanline == PRERENDER_SCANLINE && (pixel == PIXELS_PER_LINE || (pixel == (PIXELS_PER_LINE-1) && !isOdd))) {
		pixel = 0;
		scanline = 0;
		frame_counter++;
	}
	else if (pixel == PIXELS_PER_LINE) {
		scanline++;
		pixel = 0;
	}

	bool isRendering = (bool)reg.ShowBGSP;
	bool isVisible = scanline < POSTRENDER_SCANLINE;
	bool isVBlank = scanline == (POSTRENDER_SCANLINE+1);
	bool isPrerender = scanline == PRERENDER_SCANLINE;

	bool isDrawing = isRendering && isVisible && ((pixel > 0 && pixel < 257) || (pixel > 320 && pixel < 337));
	bool isFetching = isRendering && (isVisible || isPrerender) && ((pixel > 1 && pixel < 256) || (pixel > 320 && pixel < 337)) && (pixel) % 8 == 0;

	if (isDrawing) {
		render_pixel();
	}

	if (isFetching) {
		load_bg_tile();
		increment_x();
	}

	if (isRendering && (isVisible || isPrerender) && pixel == 256) {
		increment_y();
	}

	if (isVBlank && pixel == 1) {
		reg.VBlank = 1;
		if (reg.NMIenabled) cpu->requestNMI();
		outputready = true;
	}
	else if (isPrerender && pixel == 1) {
		reg.VBlank = 0;
		reg.SP0hit = 0;
		reg.SPoverflow = 0;
	}

	if (isRendering && pixel == 257) {
		evaluate_sprites();
	}

	if (isRendering) {
		if ((isPrerender || isVisible) && pixel == 257) {
			h_to_v();
		}
		else if (isPrerender && (pixel >= 280 && pixel <= 304)) {
			v_to_v();
		}
	}
	cycle_counter++;
	return outputready;
}

void PPU::load_bg_tile() {
	uint16_t nametable_tile_address = (uint16_t)(0x2000 | (vram_address.data & 0x0FFF));
	uint16_t nametable_attribute_address = (uint16_t)(0x23C0 | (vram_address.data & 0x0C00) | \
		((vram_address.data >> 4) & 0x38) | ((vram_address.data >> 2) & 0x07));
	uint16_t shift = (uint16_t)((vram_address.data & 0x2) | ((vram_address.data & 0x40) >> 4));
	uint8_t attribute_bits = (uint8_t )((memory->direct_read_byte(nametable_attribute_address) >> shift) & 0x3);
	uint8_t pattern_tile = memory->direct_read_byte(nametable_tile_address);
	populateShiftRegister(pattern_tile, attribute_bits, false, vram_address.fineY);
}

void PPU::evaluate_sprites() {
    for (int i = 0; i < 256; i++) {
        fg_pixel_sp0[i] = false;
        fg_pixel_valid[i] = false;
        fg_pixel_infront[i] = false;
    }

    if (!reg.ShowSP || scanline == 0) {
        return;
    }

    int sprite_height = 8;
    if (reg.SPsize) {
        sprite_height = 16;
    }

    int num_sprites = 0;
    for (int i = 0; i < 64; i++) {
    	OAM_entry *currentSprite = memory->read_entry_OAM(uint8_t(i));
    	int y = int(currentSprite -> y_coordinate) + 1;
        int x = int(currentSprite -> x_coordinate);

        if (y >= 0xF0 || scanline+1 < y || scanline+1 >= y+sprite_height) {
            continue;
        }
        num_sprites++;

        if (num_sprites > 8) {
            reg.SPoverflow = 1;
            break;
        }

        int y_offset = scanline + 1 - y;
        uint8_t pattern_index = currentSprite ->tile_number;

        auto attributes = currentSprite->attribute;
        bool flip_h = (bool)attributes.flip_H;
        bool flip_v = (bool)attributes.flip_V;
        bool in_front = (bool)(attributes.priority == 0);

        if (flip_v) {
            y_offset = sprite_height - 1 - y_offset;
        }

        uint16_t palette_bits = uint16_t(attributes.palette & 0x3);

        populateShiftRegister(pattern_index, palette_bits, true, y_offset);



        for (int k = 0; k < 8; k++) {
            int pk = k;

            if (flip_h) {
                pk = 7 - k;
            }

            if (x + k > 0xff) {
                break;
            }

            int pos = x + k;

            if (!fg_pixel_valid[pos] && fg_sr_pixels_valid[pk]) {
                fg_pixels[pos] = fg_sr_pixels[pk];
                fg_pixel_valid[pos] = true;

                if (i == 0) {
                    fg_pixel_sp0[pos] = true;
                }

                fg_pixel_infront[pos] = in_front;
            }
        }

    }

}

void PPU::render_pixel() {
	uint32_t bgpixel = bg_pixels[x_fine_scroll];
	bool bgpixelvalid = bg_pixel_valid[x_fine_scroll];
	for (int i = 0; i < 15; i++) {
		bg_pixels[i] = bg_pixels[i+1];
		bg_pixel_valid[i] = bg_pixel_valid[i+1];
	}

	int x = pixel - 1;

	if (x >= 256) {
		return;
	}

	uint32_t finalcolor = 0;

	bool showSprites = (x >= 8 || reg.ShowSP8) && reg.ShowSP;
	bool showBackground = (x >= 8 || reg.ShowBG8) && reg.ShowBG;
	bool isBorder = x < 8 || x > 247 || scanline < 8 || scanline > 231;

	if (isBorder) {
		finalcolor = ntsc_palette[0x3F];
	}
	else if (showSprites && fg_pixel_valid[x] && (fg_pixel_infront[x] || !bgpixelvalid)) {
		finalcolor = fg_pixels[x];
	}
	else if (showBackground && bgpixelvalid) {
		finalcolor = bgpixel;
	}
	else {
		finalcolor = ntsc_palette[memory->direct_read_byte(BACKGROUND_PALETTE_ADDRESS) & 0x3F];
	}

	if (showSprites && showBackground) {
		if (bgpixelvalid && fg_pixel_valid[x] && fg_pixel_sp0[x] && x < 255) { //TODO: is this timing right?
			reg.SP0hit = 1;
		}
	}

	//frame_buffer[(scanline*SCREEN_X) + x] = finalcolor;
	int row = scanline % NUM_ROWS_IN_FRAME_BUFFER;
	int offset = row * SCREEN_X;
	uint8_t r,g,b;
	// Palette is ARGB 32b
	r = (finalcolor >> 16) & 0xFF;
	g = (finalcolor >> 8) & 0xFF;
	b = (finalcolor >> 0) & 0xFF;
	frame_buffer[offset + x] = make_color(r,g,b);

	// have filled the framebuffer, send it to the lcd
	if ((offset+x) == (FRAME_BUFFER_SIZE-1)) {
		int y = scanline - NUM_ROWS_IN_FRAME_BUFFER;
		lcd_write_frame(0, std::max(y, 0),
						SCREEN_X, NUM_ROWS_IN_FRAME_BUFFER,
						(const uint8_t*)frame_buffer);
	}

}

void PPU::populateShiftRegister(uint8_t pattern_tile, uint16_t attribute_bits, bool is_foreground, int y_offset) {
	uint16_t base_address;
	uint16_t base_palette_address;
	bool show_pixels;

	if (is_foreground) {
		if (reg.SPsize) {
			if ((pattern_tile&0x1) == 0 ){
				base_address = 0x0000;
			}
			else {
				base_address = 0x1000;
			}

			if (y_offset > 7) {
				pattern_tile |= 0x1;
				y_offset -= 8;
			}
			else {
				pattern_tile &= ~0x01;
			}
		}
		else {
			base_address = reg.SPaddr ? (uint16_t) 0x1000 : (uint16_t) 0x0000;
		}
		base_palette_address = (uint16_t)SPRITE_PALETTE_ADDRESS;
		show_pixels = (bool)reg.ShowSP;
	}
	else {
		base_address = reg.BGaddr ? (uint16_t) 0x1000 : (uint16_t) 0x0000;
		base_palette_address = (uint16_t)BACKGROUND_PALETTE_ADDRESS;
		show_pixels = (bool)reg.ShowBG;
	}

	uint8_t low = memory->direct_read_byte(base_address + (uint16_t)(pattern_tile << 4) + (uint16_t)y_offset);
	uint8_t high = memory->direct_read_byte(base_address + (uint16_t)(pattern_tile << 4) + (uint16_t)y_offset + (uint16_t)8);

	for (int i = 0; i < 8; i++) {
		bool lowBit = (bool)((low >> unsigned(7-i)) & 0x1);
		bool highBit = (bool)((high >> unsigned(7-i)) & 0x1);

		uint16_t index = 0;
		if (highBit) {
			index |= 0x2;
		}
		if (lowBit) {
			index |= 0x1;
		}
		if (is_foreground) {
			if (index == 0 || !show_pixels) {
				fg_sr_pixels[i] = 0;
				fg_sr_pixels_valid[i] = false;
			} else {
				uint8_t palette_index = (
						memory->direct_read_byte(base_palette_address | (attribute_bits << 2) | index) & (uint8_t) 0x3F);
				fg_sr_pixels[i] = ntsc_palette[palette_index];
				fg_sr_pixels_valid[i] = true;
			}
		}
		else {

			if (index == 0 || !show_pixels) {
				bg_pixels[i + 8] = 0;
				bg_pixel_valid[i + 8] = false;
			} else {
				uint8_t palette_index = (
						memory->direct_read_byte(base_palette_address + (attribute_bits << 2) + index) & (uint8_t) 0x3F);
				bg_pixels[i + 8] = ntsc_palette[palette_index];
				bg_pixel_valid[i + 8] = true;
			}
		}
	}
}

void PPU::increment_x() {
    if (vram_address.coarseX == 31) {
		vram_address.data &= ~0x001F;
		vram_address.data ^= 0x0400;
    }
    else  {
    	vram_address.coarseX++;
    }
}

void PPU::increment_y() {
	if (vram_address.fineY < 7) {
		vram_address.fineY++;
	}
	else {
		vram_address.fineY = 0;
		int y = vram_address.coarseY;
		if (y == 29) {
			y = 0;
			vram_address.data ^= 0x0800;
		}
		else if (y == 31) {
			y = 0;
		}
		else {
			y += 1;
		}
		vram_address.coarseY = y;
	}
}

void PPU::h_to_v() {
	vram_address.data = (vram_address.data & 0xFBE0) | (temp_vram_address.data & ~0xFBE0);
}

void PPU::v_to_v() {
	vram_address.coarseY = temp_vram_address.coarseY;
	vram_address.fineY = temp_vram_address.fineY;
	vram_address.NTselectY = temp_vram_address.NTselectY;
}

