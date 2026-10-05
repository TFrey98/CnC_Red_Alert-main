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
**	LCWUNCMP.CPP -- PORT-CREATED. C translation of LCWUNCMP.ASM
**	(LCW_Uncompress), the LCW / "Format80" decompressor the shipped game
**	linked. Verified against the original assembly -- this file's and the
**	movie player's WINVQ/VQM32/LCWUNCMP.ASM, which behave the same -- in
**	port/tests/asm_misc.cpp.
**
**	It replaces CODE/LCWUNCMP.CPP in the build. That file is Westwood's own C
**	version, written "for cross-platform compatibility", and its header says
**	the third argument is a dummy: it decodes until the 0x80 end marker and
**	never looks at the length. The assembly does the opposite, and the game
**	depends on it: it stops as soon as `length` bytes are out, before reading
**	another command, and clamps every copy and run to the space left. Pictures
**	whose compressed data ends exactly at the end of the load buffer (no end
**	marker needed) made the C version read, and then act on, a byte past it.
**	Found by RA_SANITIZE=1.
**
**	The assembly's dword fast paths (rep stosd / rep movsd, with an unaligned
**	first dword) are byte-for-byte the same as plain byte loops here: the
**	movsd path is taken only when the source is at least four bytes behind
**	the destination, so every byte it reads is already final.
*/

#include <stdint.h>
#include <string.h>

/*
**	Command bytes, as the assembly documents them:
**	  0xxxyyyy,yyyyyyyy      short run   copy x+3 bytes from y bytes back
**	  10xxxxxx,n1..nx        medium      copy the next x bytes as they are
**	  11xxxxxx,w1            med run     copy x+3 bytes from offset w1
**	  11111111,w1,w2         long copy   copy w1 bytes from offset w2
**	  11111110,w1,b1         long run    write byte b1, w1 times
**	  10000000               end
**	Offsets in the run forms are from the start of the destination.
*/
extern "C" unsigned long __cdecl LCW_Uncompress(void * source, void * dest, unsigned long length)
{
	unsigned char const * s = (unsigned char const *)source;
	unsigned char * const first = (unsigned char *)dest;
	unsigned char * d = first;
	unsigned char * const last = first + length;

	for (;;) {
		uint32_t const maxlen = (uint32_t)(last - d);
		if (maxlen == 0) break;							// output full: done, without reading on

		unsigned op = *s++;
		uint32_t count;

		if (!(op & 0x80)) {								// short run, relative
			count = (op >> 4) + 3;
			if (count > maxlen) count = maxlen;
			unsigned char const * from = d - (((op & 0x0F) << 8) | *s++);
			while (count--) *d++ = *from++;
			continue;
		}

		if (!(op & 0x40)) {
			if (op == 0x80) break;						// end
			count = op & 0x3F;								// medium: literal bytes
			if (count > maxlen) count = maxlen;
			while (count--) *d++ = *s++;
			continue;
		}

		if (op == 0xFE) {									// long run of one byte
			count = (uint32_t)s[0] | ((uint32_t)s[1] << 8);
			unsigned char const value = s[2];
			s += 3;
			if (count > maxlen) count = maxlen;
			memset(d, value, count);
			d += count;
			continue;
		}

		if (op == 0xFF) {									// long copy
			count = (uint32_t)s[0] | ((uint32_t)s[1] << 8);
			s += 2;
		} else {												// medium run
			count = (op & 0x3F) + 3;
		}
		unsigned char const * from = first + ((uint32_t)s[0] | ((uint32_t)s[1] << 8));
		s += 2;
		if (count > maxlen) count = maxlen;
		while (count--) *d++ = *from++;
	}

	return (unsigned long)(d - first);
}
