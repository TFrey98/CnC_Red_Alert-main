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
**	PAL.CPP -- PORT-CREATED. C translation of PAL.ASM (Set_Palette_Range,
**	Bump_Color) and the palette state it defined. Verified against the
**	original assembly: port/tests/asm_misc.cpp.
*/

#include "wwstd.h"
#include "palette.h"
#include "misc.h"
#include <string.h>

/*
**	PAL.ASM's data segment. CurrentPalette starts all 255s, as it did.
*/
static struct CurrentPaletteInit {
	CurrentPaletteInit() {memset(CurrentPalette, 255, 768);}
} CurrentPaletteInitializer;
extern "C" {
unsigned char	CurrentPalette[768];
unsigned char	PaletteTable[1024];
}

/*
**	The Win95 version (the assembly's `IF 1` branch): remember the palette and
**	hand it to the display layer. The DOS VGA-register version beside it was
**	never built.
*/
extern "C" void __cdecl Set_Palette_Range(void * palette)
{
	memcpy(CurrentPalette, palette, 768);
	Set_DD_Palette(palette);
}

/*
**	Move colour `changable` one step (per component) toward colour `target`;
**	TRUE if anything changed. Both indices are 16-bit (the assembly's WORD
**	arguments). The direction comes from the borrow of the 8-bit subtraction.
*/
extern "C" BOOL __cdecl Bump_Color(void * palette, int changable, int target)
{
	unsigned char * cur = (unsigned char *)palette + 3 * (uint32_t)(uint16_t)changable;
	unsigned char const * want = (unsigned char const *)palette + 3 * (uint32_t)(uint16_t)target;
	BOOL changed = FALSE;
	for (int c = 0; c < 3; c++) {
		if (want[c] == cur[c]) continue;
		changed = TRUE;
		if (want[c] > cur[c]) cur[c]++; else cur[c]--;
	}
	return changed;
}
