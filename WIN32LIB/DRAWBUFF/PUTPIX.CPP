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
**	PUTPIX.CPP -- PORT-CREATED. C translation of PUTPIX.ASM (Buffer_Put_Pixel).
**	Verified against the original assembly: port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"

/*
**	x and y are compared UNSIGNED against the view size (`jae`), so negative
**	coordinates are rejected along with ones past the edge.
*/
void WWPort_Buffer_Put_Pixel(WWPortView const & v, int x, int y, unsigned char color)
{
	if ((uint32_t)x >= (uint32_t)v.Width) return;
	if ((uint32_t)y >= (uint32_t)v.Height) return;
	v.Offset[x + (size_t)y * (uint32_t)v.Stride()] = color;
}

extern "C" void __cdecl Buffer_Put_Pixel(void * thisptr, int x, int y, unsigned char color)
{
	WWPort_Buffer_Put_Pixel(WWPort_View_Of((GraphicViewPortClass *)thisptr), x, y, color);
}
