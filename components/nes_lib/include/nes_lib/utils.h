//
// Created by Ethan Williams on 2019-01-31.
//

#ifndef NESEMU_UTILS_H
#define NESEMU_UTILS_H

#include <cstdint>

#define KILO 1024

/* struct RegBit taken from https://bisqwit.iki.fi/jutut/kuvat/programming_examples/nesemu1/nesemu1.cc */
// Bitfield utilities
template<unsigned bitno, unsigned nbits=1, typename T=uint8_t>
struct RegBit
{
		T data;
		enum { mask = (1u << nbits) - 1u };
		template<typename T2>
		RegBit& operator=(T2 val)
		{
			data = (data & ~(mask << bitno)) | ((nbits > 1 ? val & mask : !!val) << bitno);
			return *this;
		}
		operator unsigned() const { return (data >> bitno) & mask; }
		RegBit& operator++ ()     { return *this = *this + 1; }
		unsigned operator++ (int) { unsigned r = *this; ++*this; return r; }
};

#endif //NESEMU_UTILS_H
