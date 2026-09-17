#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <math.h>
#include <assert.h>

// Set to 1 to assert on overflow, to ferret out precision issues with the Classic Renderer.
// Note, however, that overflow is expected when running at 320x200, so this should be 0 when making 
// releases.
#define ASSERT_ON_OVERFLOW 0

namespace TFE_Jedi
{
	// Constants
	#define HALF_16 0x8000
	#define ONE_16  0x10000
	#define ANGLE_MASK 0x3fff
	#define ANGLE_MAX  0x4000

	#define FRAC_BITS_16 16ll
	#define FRAC_MASK_16 ((1 << FRAC_BITS_16) - 1)
	#define FLOAT_SCALE_16 65536.0f
	#define INV_FLOAT_SCALE_16 (1.0f/FLOAT_SCALE_16)
	#define ANGLE_TO_FIXED_SCALE 4

	// Fixed point type
	typedef s32 fixed16_16;		// 16.16 fixed point
	typedef s32 angle14_32;		// 14-bit angle in 32-bits (maps 360 degrees to 16384 angle units).
	typedef s16 angle14_16;		// 14-bit angle in 16-bits (maps 360 degrees to 16384 angle units).

	// Simple way of setting readable fixed point constants.
	#define FIXED(x) ((x) << FRAC_BITS_16)
	
	// multiplies 2 fixed point numbers, the result is fixed point.
	// 16.16 * 16.16 overflows 32 bit, so the calculation is done in 64 bit and then shifted back to 32 bit.
#if defined(TFE_ESPBOX) && defined(__mc68060__)
#ifdef CLIB_DEBUG_PROTOS_H
	inline fixed16_16 mul16_fix(fixed16_16 x, fixed16_16 y)
	{
		return fixed16_16((s64(x) * s64(y)) >> FRAC_BITS_16);
	}
	inline fixed16_16 mul16_flt(fixed16_16 x, fixed16_16 y)
	{
		return fixed16_16((f64(x) * f64(y)) * (1.0 / 65536.0));
	}
	inline fixed16_16 mul16_dbg(fixed16_16 x, fixed16_16 y, const char *func, int line)
	{
		fixed16_16 fix = mul16_fix(x,y);
		fixed16_16 flt = mul16_flt(x,y);
		if (fix != flt)
			kprintf("%s:%ld %08x %08x\n", func, line, fix, flt);
		return flt;
	}
	//#define mul16(x,y) mul16_dbg((x),(y),__FUNCTION__,__LINE__)
	#define mul16(x,y) mul16_flt((x),(y))
#else
	inline fixed16_16 mul16_fix(fixed16_16 x, fixed16_16 y)
	{
		return fixed16_16((s64(x) * s64(y)) >> FRAC_BITS_16);
	}

	inline fixed16_16 mul16(fixed16_16 x, fixed16_16 y)
	{
		/*
		const int precision = 0;
		const f64 x64 = f64(x >> precision);
		const f64 y64 = f64(y >> precision);

		//return fixed16_16((x64 * y64) * (f64(1 << (precision * 2)) / 65536.0));
		return fixed16_16((x64 * y64) * (1.0 / 65536.0)) << (precision * 2);
		*/
		return fixed16_16((f64(x) * f64(y)) * (1.0 / 65536.0));
	}
#endif
#else
	inline fixed16_16 mul16(fixed16_16 x, fixed16_16 y)
	{
		const s64 x64 = s64(x);
		const s64 y64 = s64(y);

		// Overflow precision test.
		#if ASSERT_ON_OVERFLOW == 1
		assert(((x64 * y64) >> FRAC_BITS_16) == fixed16_16((x64 * y64) >> FRAC_BITS_16));
		#endif

		return fixed16_16((x64 * y64) >> FRAC_BITS_16);
	}
#endif

	// divides 2 fixed point numbers, the result is fixed point.
	// 16.16 * FIXED_ONE overflows 32 bit, so the calculation is done in 64 bit but the result can be safely cast back to 32 bit.
#if defined(TFE_ESPBOX) && defined(__mc68060__)
	inline fixed16_16 div16(fixed16_16 num, fixed16_16 denom)
	{
		const f64 num64 = f64(num);
		const f64 den64 = f64(denom);

		return fixed16_16((num64 / den64) * 65536.0);
	}
#else
	inline fixed16_16 div16(fixed16_16 num, fixed16_16 denom)
	{
		const s64 num64 = s64(num);
		const s64 den64 = s64(denom);

		// Overflow precision test.
		#if ASSERT_ON_OVERFLOW == 1
		assert(((num64 << FRAC_BITS_16) / den64) == fixed16_16((num64 << FRAC_BITS_16) / den64));
		#endif

		return fixed16_16((num64 << FRAC_BITS_16) / den64);
	}
#endif

	// truncates a 16.16 fixed point number, returns an int: x >> 16
	inline s32 floor16(fixed16_16 x)
	{
		return s32(x >> FRAC_BITS_16);
	}

	inline fixed16_16 fract16(fixed16_16 x)
	{
		return x & FRAC_MASK_16;
	}

	// computes a * b / c while keeping everything in 64 bits until the end.
	inline fixed16_16 fusedMulDiv(fixed16_16 a, fixed16_16 b, fixed16_16 c)
	{
		const s64 a64 = s64(a);
		const s64 b64 = s64(b);
		const s64 c64 = s64(c);
		s64 value = (a64 * b64) / c64;

		// Overflow precision test.
		#if ASSERT_ON_OVERFLOW == 1
		assert(((a64 * b64) / c64) == fixed16_16((a64 * b64) / c64));
		#endif

		return fixed16_16(value);
	}

	// rounds a 16.16 fixed point number, returns an int: (x + HALF_16) >> 16
	inline s32 round16(fixed16_16 x)
	{
		return s32((x + HALF_16) >> FRAC_BITS_16);
	}

	// converts an integer to a fixed point number: x << 16
	inline fixed16_16 intToFixed16(s32 x)
	{
		return fixed16_16(x) << FRAC_BITS_16;
	}

	inline fixed16_16 floatToFixed16(f32 x)
	{
		return fixed16_16(x * FLOAT_SCALE_16);
	}

	inline f32 fixed16ToFloat(fixed16_16 x)
	{
		return f32(x) * INV_FLOAT_SCALE_16;
	}

	inline angle14_16 floatToAngle(f32 angle)
	{
		return angle14_16(angle * 45.5111f);
	}

	// Convert a floating point angle from [0, 2pi) to a fixed point value where 0 -> 0 and 2pi -> 16384
	// This isn't really the same as fixed16_16 but matches the original source.
	inline fixed16_16 floatAngleToFixed(f32 angle)
	{
		return fixed16_16(16384.0f * angle / (2.0f*PI));
	}

	// Convert a floating point angle from [0, 2pi) to a fixed point value where 0 -> 0 and 2pi -> 16384
	// This isn't really the same as fixed16_16 but matches the original source.
	inline fixed16_16 floatDegreesToFixed(f32 angle)
	{
		return fixed16_16(16384.0f * angle / 360.0f);
	}

	inline s32 fixed16to12(fixed16_16 x)
	{
		return s32(x >> 4);
	}

#ifdef TFE_ESPBOX
/*
MIT License

Copyright (c) 2019 Christophe Meessen

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
	inline fixed16_16 fixedSqrt(fixed16_16 x)
	{
		u32 t, q, b, r;
		r = x;
		b = 0x40000000;
		q = 0;
		while (b > 0x40)
		{
			t = q + b;
			if (r >= t)
			{
				r -= t;
				q = t + b; // equivalent to q += 2*b
			}
			r <<= 1;
			b >>= 1;
		}
		q >>= 8;
		return q;
	}
#else
	// I cheat here with the fixedSqrt and just do a regular square root...
	// TODO: Replace with 15-bit table-based sqrt? -- Also probably not necessary.
	inline fixed16_16 fixedSqrt(fixed16_16 x)
	{
		const f32 fx = fixed16ToFloat(x);
		return floatToFixed16(sqrtf(fx));
	}
#endif
}
