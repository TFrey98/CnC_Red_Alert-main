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
**	FILLRECT.CPP -- PORT-CREATED. C translation of FILLRECT.ASM
**	(Buffer_Fill_Rect). Verified against the original assembly:
**	port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"
#include <string.h>

/*
**	Fill the rectangle with corners (sx,sy)-(dx,dy), inclusive, in either order,
**	clipped to the view. Faithful to the assembly's arithmetic:
**
**	  * Coordinates are 16-bit: it loads them with `movsx reg, word`, so only
**	    the low 16 bits of each int count.
**	  * Clipping mixes signed and unsigned compares exactly as below; a
**	    rectangle wholly off one edge ends with a width/height that is zero or
**	    "negative" (huge unsigned) and draws nothing.
**	  * Its contiguous fast path (no gap between lines) and dword-aligned path
**	    write the same bytes as the plain loop.
*/
void WWPort_Buffer_Fill_Rect(WWPortView const & v, int sx, int sy, int dx, int dy, unsigned char color)
{
	int32_t x = (int16_t)sx, x2 = (int16_t)dx;
	int32_t y = (int16_t)sy, y2 = (int16_t)dy;
	if (x >= x2) {int32_t t = x; x = x2; x2 = t;}
	if (y >= y2) {int32_t t = y; y = y2; y2 = t;}
	int32_t w = x2 - x + 1;
	int32_t h = y2 - y + 1;

	if (x >= v.Width) return;
	if (x < 0) {w += x; x = 0;}
	if (y >= v.Height) return;
	if (y < 0) {h += y; y = 0;}

	int32_t under = (int32_t)((uint32_t)v.Width - (uint32_t)x - (uint32_t)w);
	if (under < 0) w += under;
	under = (int32_t)((uint32_t)v.Height - (uint32_t)y - (uint32_t)h);
	if (under < 0) h += under;

	if (w == 0 || h == 0) return;
	if ((uint32_t)w > (uint32_t)v.Width) return;
	if ((uint32_t)h > (uint32_t)v.Height) return;

	unsigned char * p = v.Offset + (size_t)y * (uint32_t)v.Stride() + x;
	for (int32_t row = 0; row < h; row++) {
		memset(p, color, (uint32_t)w);
		p += v.Stride();
	}
}

extern "C" void __cdecl Buffer_Fill_Rect(void * thisptr, int sx, int sy, int dx, int dy, unsigned char color)
{
	WWPort_Buffer_Fill_Rect(WWPort_View_Of((GraphicViewPortClass *)thisptr), sx, sy, dx, dy, color);
}
