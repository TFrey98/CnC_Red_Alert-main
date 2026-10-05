/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
**	AUDUNZAP.CPP -- PORT-CREATED. C translation of AUDUNZAP.ASM (AudioUnzap),
**	the movie-audio decoder. Verified against the original assembly:
**	port/tests/asm_misc.cpp. The same decoder as WIN32LIB/AUDIO/AUDUNCMP.CPP
**	(the assembly was duplicated too), minus that one's NULL / zero checks.
*/

#include <vqm32/compress.h>
#include <stdint.h>
#include <string.h>

/*
**	Westwood's 2/4-bit delta audio ("SND1"). Each code byte is cccnnnnnn: the
**	top two bits pick the form, the low six a count.
**	  0  2-bit deltas: n+1 bytes, four samples each (bits 1-0 first)
**	  1  4-bit deltas: n+1 bytes, two samples each (low nibble first)
**	  2  n & 0x20: one sample, previous + the 5-bit signed delta n (NOT clamped)
**	     else:     n+1 raw sample bytes
**	  3  n+1 copies of the previous sample
**	The 2/4-bit steps saturate at 0 and 255 the way the assembly did it: by the
**	carry of an 8-bit add of the (signed) delta. The previous sample starts at
**	0x80. Whole codes are always written, so the output can run up to three
**	samples past `count`; the return value is the number of bytes consumed.
*/
static const signed char Decode2[4] = {-2, -1, 0, 1};
static const signed char Decode4[16] = {-9, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8};

static inline unsigned char ww_step(unsigned char prev, signed char delta)
{
	unsigned sum = (unsigned)prev + (unsigned char)delta;
	bool carry = sum > 0xFF;
	unsigned char r = (unsigned char)sum;
	if (delta >= 0) {
		if (carry) r = 0xFF;
	} else {
		if (!carry) r = 0;
	}
	return r;
}

static int32_t ww_unzap(unsigned char const * s, unsigned char * d, int32_t count)
{
	int32_t incount = 0;
	unsigned char prev = 0x80;
	while (count > 0) {
		unsigned char b = *s++;
		incount++;
		unsigned code = b >> 6;
		unsigned n = b & 0x3F;
		switch (code) {
			case 2:
				if (n & 0x20) {
					prev = (unsigned char)(prev + (unsigned char)((signed char)(n << 3) >> 3));
					*d++ = prev;
					count--;
				} else {
					memcpy(d, s, n + 1);
					s += n + 1;
					d += n + 1;
					incount += n + 1;
					count -= n + 1;
					prev = d[-1];
				}
				break;
			case 1:
				for (unsigned i = 0; i <= n; i++) {
					unsigned char v = *s++;
					incount++;
					unsigned char lo = ww_step(prev, Decode4[v & 15]);
					unsigned char hi = ww_step(lo, Decode4[v >> 4]);
					*d++ = lo;
					*d++ = hi;
					count -= 2;
					prev = hi;
				}
				break;
			case 0:
				for (unsigned i = 0; i <= n; i++) {
					unsigned char v = *s++;
					incount++;
					for (int k = 0; k < 8; k += 2) {
						prev = ww_step(prev, Decode2[(v >> k) & 3]);
						*d++ = prev;
					}
					count -= 4;
				}
				break;
			default:
				memset(d, prev, n + 1);
				d += n + 1;
				count -= n + 1;
				break;
		}
	}
	return incount;
}

extern "C" long __cdecl AudioUnzap(void * source, void * dest, long size)
{
	return ww_unzap((unsigned char const *)source, (unsigned char *)dest, (int32_t)size);
}
