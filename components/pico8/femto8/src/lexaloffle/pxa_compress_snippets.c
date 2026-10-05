/*

	pxa compression snippets for PICO-8 cartridge format (as of 0.2.4c)

	author: joseph@lexaloffle.com

	Copyright (c) 2020-22  Lexaloffle Games LLP

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.

*/

#include <stdlib.h>
#include <string.h>

#include "pico8.h"

#ifndef MAX
	#define MAX(x, y) (((x) > (y)) ? (x) : (y))
	#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif


// 3 3 5 4  (gives balanced trees for typical data)

#define PXA_MIN_BLOCK_LEN 3
#define BLOCK_LEN_CHAIN_BITS 3
#define BLOCK_DIST_BITS 5
#define TINY_LITERAL_BITS 4


// has to be 3 (optimized in find_repeatable_block)
#define MIN_BLOCK_LEN 3
#define HASH_MAX 4096
#define MINI_HASH(pp, i) ((pp[i+0]*7 + pp[i+1]*1503 + pp[i+2]*51717) & (HASH_MAX-1))

typedef unsigned short int uint16;
typedef unsigned char uint8;

#define WRITE_VAL(x) {*p_8 = (x); p_8++;}


static int bit = 1;
static int byte = 0;
static int src_pos = 0;


//-------------------------------------------------
// pxa bit-level read/write help functions
//-------------------------------------------------
;
static uint8 *src_buf = NULL;


static int getbit()
{
	int ret;

	ret = (src_buf[src_pos] & bit) ? 1 : 0;
	bit <<= 1;
	if (bit == 256)
	{
		bit = 1;
		src_pos ++;
	}
	return ret;
}

static int getval(int bits)
{
	int i;
	int val = 0;
	if (bits == 0) return 0;

	for (i = 0; i < bits; i++)
		if (getbit())
			val |= (1 << i);

	return val;
}


static int getchain(int link_bits, int max_bits)
{
	int max_link_val = (1 << link_bits) - 1;
	int val = 0;
	int vv = max_link_val;
	int bits_read = 0;

	while (vv == max_link_val)
	{
		vv = getval(link_bits);
		bits_read += link_bits;
		val += vv;
		if (bits_read >= max_bits) return val; // next val is implicitly 0
	}

	return val;
}


static int getnum()
{
	int jump = BLOCK_DIST_BITS;
	int bits = jump;
	int val;

	// 1  15 bits // more frequent so put first
	// 01 10 bits
	// 00  5 bits
	bits = (3 - getchain(1, 2)) * BLOCK_DIST_BITS;

	val = getval(bits);

	if (val == 0 && bits == 10)
		return -1; // raw block marker

	return val;
}

// ---------------------


#define PXA_WRITE_VAL(x) {literal_bits_written += putval(x,8);}
#define PXA_READ_VAL(x)  getval(8)


static void init_literals_state(int *literal, int *literal_pos)
{
	int i;

	// starting state makes little difference
	// using 255-i (which seems terrible) only costs 10 bytes more.

	for (i = 0; i < 256; i++)
		literal[i] = i;

	for (i = 0; i < 256; i++)
		literal_pos[literal[i]] = i;
}


#define BACKUP_VLIST_STATE()  memcpy(literal_backup, literal, sizeof(literal));  memcpy(literal_pos_backup, literal_pos, sizeof(literal_pos));
#define RESTORE_VLIST_STATE() memcpy(literal, literal_backup, sizeof(literal));  memcpy(literal_pos, literal_pos_backup, sizeof(literal_pos));


int pxa_decompress(uint8 *in_p, uint8 *out_p, int max_len)
{
	int i;
	int literal[256];
	int literal_pos[256];
	int dest_pos = 0;

	bit = 1;
	byte = 0;
	src_buf = in_p;
	src_pos = 0;

	init_literals_state(literal, literal_pos);

	// header

	int header[8];
	for (i = 0; i < 8; i++)
		header[i] = PXA_READ_VAL();

	int raw_len  = header[4] * 256 + header[5];
	int comp_len = header[6] * 256 + header[7];

	// printf(" read raw_len:  %d\n", raw_len);
	// printf(" read comp_len: %d\n", comp_len);

	while (src_pos < comp_len && dest_pos < raw_len && dest_pos < max_len)
	{
		int block_type = getbit();

		// printf("%d %d\n", src_pos, block_type); fflush(stdout);

		if (block_type == 0)
		{
			// block

			int block_offset = getnum() + 1;

			if (block_offset == 0)
			{
				// 0.2.0j: raw block
				while (dest_pos < raw_len)
				{
					out_p[dest_pos] = getval(8);
					if (out_p[dest_pos] == 0) // found end -- don't advance dest_pos
						break;
					dest_pos ++;
				}
			}
			else
			{
				int block_len = getchain(BLOCK_LEN_CHAIN_BITS, 100000) + PXA_MIN_BLOCK_LEN;

				// copy // don't just memcpy because might be copying self for repeating pattern
				while (block_len > 0){
					out_p[dest_pos] = out_p[dest_pos - block_offset];
					dest_pos++;
					block_len--;
				}

				// safety: null terminator. to do: just do at end
				if (dest_pos < max_len-1)
					out_p[dest_pos] = 0;
			}
		}else
		{
			// literal

			int lpos = 0;
			int bits = 0;

			int safety = 0;
			while (getbit() == 1 && safety++ < 16)
			{
				lpos += (1 << (TINY_LITERAL_BITS + bits));
				bits ++;
			}

			bits += TINY_LITERAL_BITS;
			lpos += getval(bits);

			if (lpos > 255) return 0; // something wrong

			// grab character and write
			int c = literal[lpos];

			out_p[dest_pos] = c;
			dest_pos++;
			out_p[dest_pos] = 0;

			int i;
			for (i = lpos; i > 0; i--)
			{
				literal[i] = literal[i-1];
				literal_pos[literal[i]] ++;
			}
			literal[0] = c;
			literal_pos[c] = 0;
		}
	}


	return 0;
}

int is_compressed_format_header(uint8 *dat)
{
	if (dat[0] == ':' && dat[1] == 'c' && dat[2] == ':' && dat[3] == 0) return 1;
	if (dat[0] == 0 && dat[1] == 'p' && dat[2] == 'x' && dat[3] == 'a') return 2;
	return 0;
}

// max_len should be 0x10000 (64k max code size)
// out_p should allocate 0x10001 (includes null terminator)
int pico8_code_section_decompress(uint8 *in_p, uint8 *out_p, int max_len)
{
	if (is_compressed_format_header(in_p) == 0) { memcpy(out_p, in_p, 0x3d00); out_p[0x3d00] = '\0'; return 0; } // legacy: no header -> is raw text
	if (is_compressed_format_header(in_p) == 1) return decompress_mini(in_p, out_p, max_len);
	if (is_compressed_format_header(in_p) == 2) return pxa_decompress (in_p, out_p, max_len);
	return 0;
}
