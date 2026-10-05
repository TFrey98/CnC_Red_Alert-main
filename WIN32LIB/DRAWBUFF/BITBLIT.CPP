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
**	BITBLIT.CPP -- PORT-CREATED. C translation of BITBLIT.ASM
**	(Linear_Blit_To_Linear). Verified against the original assembly, including
**	overlapping blits within one buffer: port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"
#include <string.h>

/*
**	Copy a pixel_width x pixel_height region from (x,y) in one view to
**	(dx,dy) in another (or the same) view, clipped to both. With `trans`,
**	colour 0 is transparent. Faithful to the assembly, including:
**
**	  * Clipping the SOURCE on the left or top does not move the destination:
**	    the source start becomes 0 but dx/dy stay, so the visible part lands
**	    shifted. (Clipping against the destination does adjust the source.)
**	  * Overlap is handled by direction, chosen by comparing the two start
**	    addresses: source after destination copies forward (rows top to bottom,
**	    left to right); source before destination copies backward (bottom to
**	    top, right to left). Equal start addresses copy nothing.
**	  * The return value is what the assembly left in EAX: the copied width, or
**	    width / 32 after a transparent copy, or register leftovers on the early
**	    exits. Reproduced so the tests can hold it exact.
*/
int WWPort_Linear_Blit_To_Linear(WWPortView const & src, WWPortView const & dst, int x, int y,
	int dx, int dy, int pixel_width, int pixel_height, int trans)
{
	int32_t x0 = x, y0 = y;
	int32_t x1 = (int32_t)((uint32_t)x + (uint32_t)pixel_width);
	int32_t y1 = (int32_t)((uint32_t)y + (uint32_t)pixel_height);
	unsigned c0 = WWPort_Outcode(x0, y0, src.Width, src.Height);
	unsigned c1 = WWPort_Outcode(x1, y1, src.Width, src.Height);
	if (c0 & c1) return (int32_t)((((uint32_t)y0 - (uint32_t)(src.Height + 1)) & ~0xFFu) | c0);
	if (c0 & 8) x0 = 0;
	if (c0 & 2) y0 = 0;
	if (c1 & 4) x1 = src.Width;
	if (c1 & 1) y1 = src.Height;

	int32_t dx0 = dx, dy0 = dy;
	int32_t dx1 = (int32_t)((uint32_t)dx0 - (uint32_t)x0 + (uint32_t)x1);
	int32_t dy1 = (int32_t)((uint32_t)dy0 - (uint32_t)y0 + (uint32_t)y1);
	c0 = WWPort_Outcode(dx0, dy0, dst.Width, dst.Height);
	c1 = WWPort_Outcode(dx1, dy1, dst.Width, dst.Height);
	if (c0 & c1) return (int32_t)((((uint32_t)dy0 - (uint32_t)(dst.Height + 1)) & ~0xFFu) | c0);
	if (c0 & 8) {x0 = (int32_t)((uint32_t)x0 - (uint32_t)dx0); dx0 = 0;}
	if (c0 & 2) {y0 = (int32_t)((uint32_t)y0 - (uint32_t)dy0); dy0 = 0;}
	if (c1 & 4) {x1 = (int32_t)((uint32_t)x1 - ((uint32_t)dx1 - (uint32_t)dst.Width)); dx1 = dst.Width;}
	if (c1 & 1) {y1 = (int32_t)((uint32_t)y1 - ((uint32_t)dy1 - (uint32_t)dst.Height)); dy1 = dst.Height;}

	uint32_t const sstride = (uint32_t)src.Stride();
	uint32_t const dstride = (uint32_t)dst.Stride();
	unsigned char * s = src.Offset + x0 + (int32_t)(sstride * (uint32_t)y0);
	unsigned char * d = dst.Offset + dx0 + (int32_t)(dstride * (uint32_t)dy0);

	/* `sub / jle` is a signed comparison of the operands, overflow included. */
	int32_t width = (int32_t)((uint32_t)dx1 - (uint32_t)dx0);
	if (dx1 <= dx0) return width;
	if (dy1 <= dy0) return width;
	int32_t rows = (int32_t)((uint32_t)dy1 - (uint32_t)dy0);
	if (s == d) return width;

	if (s > d) {
		for (int32_t r = 0; r < rows; r++) {
			if (trans & 1) {
				for (int32_t i = 0; i < width; i++) if (s[i]) d[i] = s[i];
			} else {
				memmove(d, s, (uint32_t)width);
			}
			s += sstride;
			d += dstride;
		}
	} else {
		s += (size_t)sstride * (rows - 1);
		d += (size_t)dstride * (rows - 1);
		for (int32_t r = 0; r < rows; r++) {
			if (trans & 1) {
				for (int32_t i = width - 1; i >= 0; i--) if (s[i]) d[i] = s[i];
			} else {
				memmove(d, s, (uint32_t)width);
			}
			s -= sstride;
			d -= dstride;
		}
	}
	return (trans & 1) ? (int32_t)((uint32_t)width >> 5) : width;
}

extern "C" BOOL __cdecl Linear_Blit_To_Linear(void * thisptr, void * dest, int x_pixel, int y_pixel, int dx_pixel,
	int dy_pixel, int pixel_width, int pixel_height, BOOL trans)
{
	return WWPort_Linear_Blit_To_Linear(WWPort_View_Of((GraphicViewPortClass *)thisptr),
		WWPort_View_Of((GraphicViewPortClass *)dest), x_pixel, y_pixel, dx_pixel, dy_pixel,
		pixel_width, pixel_height, trans);
}
