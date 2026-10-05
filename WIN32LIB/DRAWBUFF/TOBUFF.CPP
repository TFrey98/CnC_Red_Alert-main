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
**	TOBUFF.CPP -- PORT-CREATED. C translation of TOBUFF.ASM (Buffer_To_Buffer).
**	Verified against the original assembly: port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"
#include <string.h>

/*
**	Copy a w x h region of the view into a linear buffer `w` bytes wide. Parts
**	of the region off the view are skipped, leaving those buffer bytes as they
**	were. Faithful to the assembly in three ways a reader might not expect:
**
**	  * The size check compares w * (visible rows) with buffer_size -- not the
**	    full extent written, which also includes rows skipped above the view.
**	  * TRANSP is 0 in the assembly, so its transparent-copy path is not built.
**	  * The return value is whatever the assembly left in EAX: the copied
**	    width normally, and other register leftovers on the early exits. No
**	    caller reads it, but it is reproduced so the tests can hold it exact.
*/
long WWPort_Buffer_To_Buffer(WWPortView const & v, int x, int y, int w, int h, void * buff, long size)
{
	int32_t x0 = x, y0 = y;
	int32_t x1 = (int32_t)((uint32_t)x + (uint32_t)w);
	int32_t y1 = (int32_t)((uint32_t)y + (uint32_t)h);
	int32_t dest_x = 0, dest_y = 0;
	unsigned c0 = WWPort_Outcode(x0, y0, v.Width, v.Height);
	unsigned c1 = WWPort_Outcode(x1, y1, v.Width, v.Height);
	if (c0 & c1) {
		return (int32_t)((((uint32_t)y - (uint32_t)(v.Height + 1)) & ~0xFFu) | c0);
	}
	if (c0 & 8) {dest_x = -x0; x0 = 0;}
	if (c0 & 2) {dest_y = -y0; y0 = 0;}
	if (c1 & 4) x1 = v.Width;
	if (c1 & 1) y1 = v.Height;

	/* `sub / jle` is a signed comparison of the operands, overflow included. */
	if (y1 <= y0) return x1;
	int32_t rows = (int32_t)((uint32_t)y1 - (uint32_t)y0);
	int32_t cols = (int32_t)((uint32_t)x1 - (uint32_t)x0);
	if (x1 <= x0) return cols;
	if ((int32_t)((uint32_t)w * (uint32_t)rows) > (int32_t)size) return cols;

	unsigned char const * src = v.Offset + (size_t)y0 * (uint32_t)v.Stride() + x0;
	unsigned char * dst = (unsigned char *)buff + (int32_t)((uint32_t)dest_y * (uint32_t)w + (uint32_t)dest_x);
	while (rows--) {
		memmove(dst, src, (uint32_t)cols);
		src += v.Stride();
		dst += w;
	}
	return cols;
}

extern "C" long __cdecl Buffer_To_Buffer(void * thisptr, int x, int y, int w, int h, void * buff, long size)
{
	return WWPort_Buffer_To_Buffer(WWPort_View_Of((GraphicViewPortClass *)thisptr), x, y, w, h, buff, size);
}
