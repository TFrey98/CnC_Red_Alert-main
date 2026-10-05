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
**	SETFPAL.CPP -- PORT-CREATED. C translation of SETFPAL.ASM
**	(Set_Font_Palette_Range). Verified against the original assembly:
**	port/tests/asm_misc.cpp.
*/

#include "wwstd.h"
#include "font.h"

/* The text printer's 256-byte colour table, defined in CODE/2TXTPRNT. */
extern "C" unsigned char ColorXlat[256];

/*
**	Set font colours start..end (each taken mod 16) from `palette`. Each colour
**	goes into ColorXlat twice: entry n and entry n * 16. Nothing happens if
**	end < start after the masking.
*/
extern "C" void __cdecl Set_Font_Palette_Range(void const * palette, INT start_idx, INT end_idx)
{
	unsigned char const * p = (unsigned char const *)palette;
	int start = start_idx & 0x0F;
	int end = end_idx & 0x0F;
	if (end < start) return;
	for (int i = start; i <= end; i++) {
		unsigned char c = *p++;
		ColorXlat[i] = c;
		ColorXlat[i * 16] = c;
	}
}
