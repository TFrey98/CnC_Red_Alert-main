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
**	PALETTE.CPP -- PORT-CREATED. Native replacement for the movie player's
**	PALETTE.ASM (SetPalette).
**
**	The assembly wrote `numbytes` palette bytes to the VGA DAC: index 0 to port
**	3C8h, then the 6-bit components to 3C9h (a fast `rep outsb`, or for slow
**	cards one byte at a time). The port emulates that DAC (CODE/PRAGMAUX.CPP:
**	outportb and WWPort_VGA_DAC, which the display backend presents from), so
**	this writes the same bytes through the same ports, as the game's own
**	palette code does.
**
**	(ReadPalette, SetDAC and TranslatePalette, also in PALETTE.ASM, are not
**	called by the linked game.)
*/

#include <vqm32/palette.h>

extern void outportb(int port, unsigned char data);		// CODE/PRAGMAUX.CPP

/*
**	The slow path wrote whole colours (it looped `sub cx,3 / ja`), so a byte
**	count that is not a multiple of 3 was rounded up; palettes always are one.
*/
extern "C" void __cdecl SetPalette(unsigned char * palette, long numbytes, unsigned long slowpal)
{
	long n = numbytes;
	if (slowpal) n = ((n > 0 ? n : 1) + 2) / 3 * 3;
	outportb(0x3C8, 0);
	for (long i = 0; i < n; i++) {
		outportb(0x3C9, palette[i]);
	}
}
