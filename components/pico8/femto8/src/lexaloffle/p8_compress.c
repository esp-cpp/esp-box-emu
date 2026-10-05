/*
	p8_compress.c

	(c) Copyright 2014-2016 Lexaloffle Games LLP
	author: joseph@lexaloffle.com

	compression used in code section of .p8.png format

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgment in the product documentation would be
	   appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef MAX
	#define MAX(x, y) (((x) > (y)) ? (x) : (y))
	#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

typedef unsigned char           uint8;

#define HIST_LEN 4096
#define LITERALS 60
#define PICO8_CODE_ALLOC_SIZE (0x10000+1)

#define codo_malloc malloc
#define codo_free free
#define codo_memset memset

// removed from end of decompressed if it exists
// (injected to maintain 0.1.7 forwards compatibility)
#define FUTURE_CODE "if(_update60)_update=function()_update60()_update60()end"
#define FUTURE_CODE2 "if(_update60)_update=function()_update60()_update_buttons()_update60()end"

// ^ is dummy -- not a literal. forgot '-', but nevermind! (gets encoded as rare literal)
char *literal = "^\n 0123456789abcdefghijklmnopqrstuvwxyz!#%(){}[]<>+=/*:;.,~_";
int literal_index[256]; // map literals to 0..LITERALS-1. 0 is reserved (not listed in literals string)

#define READ_VAL(val) {val = *in; in++;}
int decompress_mini(uint8 *in_p, uint8 *out_p, int max_len)
{
	int block_offset;
	int block_length;
	int val;
	uint8 *in = in_p;
	uint8 *out = out_p;
	int len;

	// header tag ":c:"
	READ_VAL(val);
	READ_VAL(val);
	READ_VAL(val);
	READ_VAL(val);

	// uncompressed length
	READ_VAL(val);
	len = val * 256;
	READ_VAL(val);
	len += val;

	// compressed length (to do: use to check)
	READ_VAL(val);
	READ_VAL(val);

	codo_memset(out_p, 0, max_len);

	if (len > max_len) return 1; // corrupt data

	while (out < out_p + len)
	{
		READ_VAL(val);

		if (val < LITERALS)
		{
			// literal
			if (val == 0)
			{
				READ_VAL(val);
				//printf("rare literal: %d\n", val);
				*out = val;
			}
			else
			{
				// printf("common literal: %d (%c)\n", literal[val], literal[val]);
				*out = literal[val];
			}
			out++;
		}
		else
		{
			// block
			block_offset = val - LITERALS;
			block_offset *= 16;
			READ_VAL(val);
			block_offset += val % 16;
			block_length = (val / 16) + 2;

			memcpy(out, out - block_offset, block_length);
			out += block_length;
		}
	}


	// remove injected code (needed to be future compatible with PICO-8 C 0.1.7 / FILE_VERSION 8)
	// older versions will leave this code intact, allowing it to implement fallback 60fps support

	if (strstr((char *)out_p, FUTURE_CODE))
	if (strlen((char *)out_p)-((char *)strstr((char *)out_p, FUTURE_CODE) - (char *)out_p) == strlen(FUTURE_CODE)) // at end
	{
		out = out_p + strlen((char *)out_p) - strlen(FUTURE_CODE);
		*out = 0;
	}

	// queue circus music
	if (strstr((char *)out_p, FUTURE_CODE2))
	if (strlen((char *)out_p)-((char *)strstr((char *)out_p, FUTURE_CODE2) - (char *)out_p) == strlen(FUTURE_CODE2)) // at end
	{
		out = out_p + strlen((char *)out_p) - strlen(FUTURE_CODE2);
		*out = 0;
	}


	return out - out_p;
}
