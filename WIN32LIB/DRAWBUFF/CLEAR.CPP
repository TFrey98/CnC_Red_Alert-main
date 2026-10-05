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
**	CLEAR.CPP -- PORT-CREATED. C translation of CLEAR.ASM (Buffer_Clear), which
**	the original kept in the same directory. Verified against the original
**	assembly run under an x86 emulator: port/asmref, port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"
#include <string.h>

/*
**	Fill every visible byte of the view with one colour, a line at a time,
**	stepping over XAdd + Pitch between lines.
**
**	The assembly's dword-aligned path (widths >= 14) writes the same bytes as
**	its byte path; only the speed differed.
**
**	Divergence: with Height == 0 the assembly's `dec edx / jnz` loop ran 2^32
**	times (a crash). No caller does that; this draws nothing instead.
*/
void WWPort_Buffer_Clear(WWPortView const & v, unsigned char color)
{
	unsigned char * p = v.Offset;
	for (int32_t y = 0; y < v.Height; y++) {
		memset(p, color, (uint32_t)v.Width);
		p += v.Stride();
	}
}

extern "C" void __cdecl Buffer_Clear(void * thisptr, unsigned char color)
{
	WWPort_Buffer_Clear(WWPort_View_Of((GraphicViewPortClass *)thisptr), color);
}
