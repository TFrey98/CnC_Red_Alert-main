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
**	LCWCOMP.CPP -- PORT-CREATED. C translation of LCWCOMP.ASM (LCW_Comp), the
**	compressor matching LCW_Uncompress; used for save games through LCWPipe /
**	LCWStraw. Verified byte-for-byte against the original assembly, and its
**	output round-trips through LCW_Uncompress: port/tests/asm_game.cpp.
*/

#include <stdint.h>
#include <string.h>		// self-contained, so library tests can link it too

/*
**	Output format (LCW, "format 80"):
**	  0x80 + n (n 1..63)      n literal bytes follow
**	  0x80                    end of data
**	  0x00..0x7F, b           copy ((c >> 4) + 3) bytes from (c & 15) << 8 | b
**	                          back from the current position
**	  0xC0 + n (n < 62), w    copy n + 3 bytes from offset w of the output
**	  0xFE, w, v              w copies of v
**	  0xFF, w1, w2            copy w1 bytes from offset w2
**
**	Faithful to the assembly's choices, so the bytes are identical:
**	  * Every position looks for the LONGEST earlier match by brute force;
**	    among equally long ones the LATEST wins.
**	  * A run is coded (0xFE) only if 65+ bytes long, and a run reaching the
**	    end of the data is counted one short of its true length.
**	  * Copies of 3..10 bytes within 4095 bytes are coded short; otherwise
**	    0xC0+ up to 64 bytes, else 0xFF. Offsets are 16-bit.
**	  * It always starts with a one-byte literal and processes at least two
**	    input bytes, reading one past the end when length < 2.
**
**	One reading is skipped rather than reproduced: the run test peeks 64
**	bytes ahead even near the end of the data. A run of 65+ needs those bytes
**	to be inside the data, so the result never depended on what was read.
*/
extern "C" int __cdecl LCW_Comp(void const * source, void * dest, int datasize)
{
	unsigned char const * const src = (unsigned char const *)source;
	unsigned char const * const end = src + (uint32_t)datasize;
	unsigned char const * s = src;
	unsigned char * const out = (unsigned char *)dest;
	unsigned char * d = out;
	unsigned char * lenoff = d;
	bool inlen = true;
	unsigned char const * matchoff = src;

	*d++ = 0x81;
	*d++ = *s++;

	for (;;) {
		unsigned char const * search = src;
		uint32_t count = 1;

		for (;;) {
			unsigned char const al = *s;
			uint32_t const left = s < end ? (uint32_t)(end - s) : 0;

			/* a run of the current byte, 65 or longer? */
			if (left > 64 && al == s[64]) {
				uint32_t k = 1;
				while (k < left && s[k] == al) k++;
				uint32_t length = (k == left) ? left - 1 : k;
				if (length >= 65) {
					*d++ = 0xFE;
					*d++ = (unsigned char)length;
					*d++ = (unsigned char)(length >> 8);
					*d++ = al;
					s += length;
					inlen = false;
					continue;
				}
			}

			/* the next earlier occurrence of this byte that could beat `count` */
			bool improved = false;
			while (search < s) {
				unsigned char const * p = (unsigned char const *)memchr(search, al, (size_t)(s - search));
				if (p == NULL) {search = s; break;}
				search = p + 1;
				if (s[count - 1] != p[count - 1]) continue;
				uint32_t n = s < end ? (uint32_t)(end - s) : 0;
				uint32_t len = 0;
				while (len < n && s[len] == p[len]) len++;
				if (len >= count) {
					count = len;
					matchoff = p;
					improved = true;
					break;
				}
			}
			if (!improved) break;
		}

		if (count <= 2) {
			if (!inlen) {
				lenoff = d;
				*d++ = 0x80;
			}
			if (*lenoff == 0xBF) {
				lenoff = d;
				*d++ = 0x80;
			}
			(*lenoff)++;
			*d++ = *s++;
			inlen = true;
		} else {
			uint32_t const dist = (uint32_t)(s - matchoff);
			uint32_t const absolute = (uint32_t)(matchoff - src);
			if (count <= 10 && dist <= 0x0FFF) {
				*d++ = (unsigned char)(((count - 3) << 4) + (dist >> 8));
				*d++ = (unsigned char)dist;
			} else if (count <= 64) {
				*d++ = (unsigned char)(0xC0 | (count - 3));
				*d++ = (unsigned char)absolute;
				*d++ = (unsigned char)(absolute >> 8);
			} else {
				*d++ = 0xFF;
				*d++ = (unsigned char)count;
				*d++ = (unsigned char)(count >> 8);
				*d++ = (unsigned char)absolute;
				*d++ = (unsigned char)(absolute >> 8);
			}
			s += count;
			inlen = false;
		}
		if (s >= end) break;
	}
	*d++ = 0x80;
	return (int)(d - out);
}
