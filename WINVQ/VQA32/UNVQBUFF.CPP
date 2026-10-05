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
**	UNVQBUFF.CPP -- PORT-CREATED. C translation of UNVQBUFF.ASM's UnVQ_4x2,
**	the VQA frame decoder for 4 x 2 blocks into a linear buffer, verified
**	against the original assembly: port/tests/asm_winvq.cpp. Also UnVQ_4x4,
**	written from the published format rather than translated (see there).
**	(The 2x2 and 2x3 variants are assembled out.)
*/

#include "unvq.h"
#include <stdint.h>
#include <string.h>

/*
**	`pointers` holds numrows * blocksperrow low bytes, then as many high bytes.
**	For each block, high byte 0x0F means "one colour": all eight pixels are
**	the low byte. Otherwise (high << 8 | low) picks an 8-byte codebook entry:
**	four pixels for the block's top row, four for the bottom. Block rows are
**	two buffer lines (2 * bufwidth) apart.
**
**	Divergence: blocksperrow == 0 made the assembly's block counter wrap
**	(2^32 iterations); this draws nothing.
*/
extern "C" void __cdecl UnVQ_4x2(unsigned char * codebook, unsigned char * pointers, unsigned char * buffer,
	unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	uint32_t const per_row = (uint32_t)blocksperrow;
	uint32_t const entries = (uint32_t)numrows * per_row;
	uint32_t const width = (uint32_t)bufwidth;
	if (per_row == 0) return;
	unsigned char const * lo = pointers;
	unsigned char const * const end = pointers + entries;
	unsigned char * row = buffer;
	do {
		unsigned char * d = row;
		for (uint32_t b = 0; b < per_row; b++, lo++, d += 4) {
			unsigned char hi = lo[entries];
			if (hi == 0x0F) {
				memset(d, *lo, 4);
				memset(d + width, *lo, 4);
			} else {
				unsigned char const * cb = codebook + (((uint32_t)hi << 8 | *lo) << 3);
				memcpy(d, cb, 4);
				memcpy(d + width, cb + 4, 4);
			}
		}
		row += 2 * width;
	} while (lo < end);
}



/*
**	4 x 4 blocks: the 640x400 Red Alert 95 start movie (ENGLISH.VQA).
**
**	NOT a translation of UNVQBUFF.ASM's UnVQ_4x4. That routine reads 16-bit
**	words as codebook byte offsets, a layout these files do not use, and the
**	released C header never enabled it (VQABLOCK_4X4 0): the film played with
**	sound and no picture. The decoder RA95 shipped for it is not in the
**	released source, so this one follows the published format (Gordan
**	Ugarkovic's VQA overview, port/PORTING.md), as UnVQ_4x2 does: the low-byte
**	table, then the high-byte table; entry (high << 8 | low) is a 16-byte
**	codeword, four rows of four pixels; and "0x0ff for the start movie of Red
**	Alert 95" (where 4x2 uses 0x0f) marks a one-colour block, its colour the low
**	byte. Verified against a reference written from that document:
**	port/tests/vqa_format.cpp.
*/
extern "C" void __cdecl UnVQ_4x4(unsigned char * codebook, unsigned char * pointers, unsigned char * buffer,
	unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	uint32_t const per_row = (uint32_t)blocksperrow;
	uint32_t const entries = (uint32_t)numrows * per_row;
	uint32_t const width = (uint32_t)bufwidth;
	if (per_row == 0) return;
	unsigned char const * lo = pointers;
	unsigned char const * const end = pointers + entries;
	unsigned char * row = buffer;
	do {
		unsigned char * d = row;
		for (uint32_t b = 0; b < per_row; b++, lo++, d += 4) {
			unsigned char hi = lo[entries];
			if (hi == 0xFF) {
				for (int y = 0; y < 4; y++) memset(d + y * width, *lo, 4);
			} else {
				unsigned char const * cw = codebook + (((uint32_t)hi << 8 | *lo) << 4);
				for (int y = 0; y < 4; y++) memcpy(d + y * width, cw + 4 * y, 4);
			}
		}
		row += 4 * width;
	} while (lo < end);
}
