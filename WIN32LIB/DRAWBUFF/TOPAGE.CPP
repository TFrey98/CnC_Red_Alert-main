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
**	TOPAGE.CPP -- PORT-CREATED. C translation of TOPAGE.ASM (Buffer_To_Page).
**	Verified against the original assembly: port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"
#include <string.h>

/*
**	Copy a linear buffer `w` bytes wide into a w x h region of the view at
**	(x,y), clipped with the assembly's outcode test. The mirror image of
**	Buffer_To_Buffer, except that it has no size check, and it preserves EAX
**	(USES eax), so its "return value" was the caller's own EAX -- meaningless.
**	TRANSP is 0 in the assembly, so its transparent path is not built.
*/
long WWPort_Buffer_To_Page(int x, int y, int w, int h, void const * buff, WWPortView const & v)
{
	if (buff == NULL) return 0;

	int32_t x0 = x, y0 = y;
	int32_t x1 = (int32_t)((uint32_t)x + (uint32_t)w);
	int32_t y1 = (int32_t)((uint32_t)y + (uint32_t)h);
	int32_t src_x = 0, src_y = 0;
	unsigned c0 = WWPort_Outcode(x0, y0, v.Width, v.Height);
	unsigned c1 = WWPort_Outcode(x1, y1, v.Width, v.Height);
	if (c0 & c1) return 0;
	if (c0 & 8) {src_x = -x0; x0 = 0;}
	if (c0 & 2) {src_y = -y0; y0 = 0;}
	if (c1 & 4) x1 = v.Width;
	if (c1 & 1) y1 = v.Height;

	/* `sub / jle` is a signed comparison of the operands, overflow included. */
	if (y1 <= y0) return 0;
	if (x1 <= x0) return 0;
	int32_t rows = (int32_t)((uint32_t)y1 - (uint32_t)y0);
	int32_t cols = (int32_t)((uint32_t)x1 - (uint32_t)x0);

	unsigned char * dst = v.Offset + (size_t)y0 * (uint32_t)v.Stride() + x0;
	unsigned char const * src = (unsigned char const *)buff + (int32_t)((uint32_t)src_y * (uint32_t)w + (uint32_t)src_x);
	while (rows--) {
		memmove(dst, src, (uint32_t)cols);
		dst += v.Stride();
		src += w;
	}
	return 0;
}

extern "C" long __cdecl Buffer_To_Page(int x, int y, int w, int h, void * buff, void * view)
{
	return WWPort_Buffer_To_Page(x, y, w, h, buff, WWPort_View_Of((GraphicViewPortClass *)view));
}
