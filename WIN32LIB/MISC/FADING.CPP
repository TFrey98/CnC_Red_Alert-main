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
**	FADING.CPP -- PORT-CREATED. C translation of FADING.ASM
**	(Build_Fading_Table). Verified against the original assembly:
**	port/tests/asm_misc.cpp.
*/

#include "wwstd.h"
#include "misc.h"

/*
**	Build a 256-byte remap table that fades every colour toward `color` by
**	frac/256 (frac is capped at 255), choosing for each the nearest palette
**	entry. Faithful to the assembly's arithmetic:
**
**	  * The ideal component is  c - high_byte(2 * (int8)(c - target) * (frac>>1)),
**	    in 8-bit arithmetic with a SIGNED 8 x 8 multiply.
**	  * Distance is the sum of squared differences, each difference taken as
**	    a signed 8-bit value.
**	  * Candidates are colours 1..255 other than the one being faded; colour 0
**	    is never chosen (and always maps to 0). Ties go to the LATER colour.
**	Returns dest, as the assembly did (also when palette or dest is NULL).
*/
extern "C" void * __cdecl Build_Fading_Table(void const * palette, void const * dest, long int color, long int frac)
{
	if (palette == NULL || dest == NULL) return (void *)dest;
	unsigned char const * pal = (unsigned char const *)palette;
	unsigned char * out = (unsigned char *)dest;
	uint32_t f = (uint32_t)frac;
	if (f >= 0x100) f = 0xFF;
	int8_t const scale = (int8_t)(f >> 1);
	unsigned char const * target = pal + (int32_t)((uint32_t)color * 3u);

	out[0] = 0;
	for (int index = 1; index <= 255; index++) {
		unsigned char ideal[3];
		for (int c = 0; c < 3; c++) {
			unsigned char v = pal[index * 3 + c];
			int16_t prod = (int16_t)((int8_t)(unsigned char)(v - target[c]) * scale);
			unsigned char ah = (unsigned char)((uint16_t)((uint16_t)prod << 1) >> 8);		// shl ax,1 / ah
			ideal[c] = (unsigned char)(v - ah);
		}

		unsigned char match = (unsigned char)color;
		uint32_t best = 0xFFFFFFFFu;
		for (int cand = 1; cand <= 255; cand++) {
			if (cand == index) continue;
			uint32_t dist = 0;
			for (int c = 0; c < 3; c++) {
				int8_t d = (int8_t)(unsigned char)(pal[cand * 3 + c] - ideal[c]);
				dist += (uint16_t)(int16_t)(d * d);
			}
			if (dist == 0) {match = (unsigned char)cand; break;}
			if (dist <= best) {best = dist; match = (unsigned char)cand;}
		}
		out[index] = match;
	}
	return (void *)dest;
}
