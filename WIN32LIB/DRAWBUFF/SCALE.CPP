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
**	SCALE.CPP -- PORT-CREATED. C translation of SCALE.ASM
**	(Linear_Scale_To_Linear). Verified against the original assembly:
**	port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "ww_view.h"

namespace {

/* imul / idiv: 64-bit signed product and quotient; false where the CPU faults (#DE). */
inline bool muldiv(int32_t a, int32_t b, int32_t d, int32_t & q)
{
	if (d == 0) return false;
	int64_t r = (int64_t)a * b / d;
	if (r < INT32_MIN || r > INT32_MAX) return false;
	q = (int32_t)r;
	return true;
}

/* `xor edx,edx / idiv d`: the 32-bit dividend is zero-extended, not sign-extended. */
inline bool udiv_by_signed(uint32_t n, int32_t d, int32_t & q, int32_t & r)
{
	if (d == 0) return false;
	int64_t qq = (int64_t)n / d;
	if (qq < INT32_MIN || qq > INT32_MAX) return false;
	q = (int32_t)qq;
	r = (int32_t)((int64_t)n % d);
	return true;
}

}

/*
**	Scale a src_width x src_height region of one view onto a dst_width x
**	dst_height region of another, nearest-pixel, optionally through a remap
**	table and/or with colour 0 transparent. Faithful to the assembly:
**
**	  * Source x steps in 16.16 fixed point, (src_width << 16) / dst_width: the
**	    fraction accumulates in the top half of a register and carries into
**	    the source pointer (`add ecx,eax / adc esi,edx`), restarting at 0 on
**	    every line -- so clipping shifts the sampling phase exactly as it did.
**	  * Source lines step Bresenham-style: src_height / dst_height whole lines
**	    per line plus one more when the accumulator (starting at -dst_height)
**	    goes positive.
**	  * Clipping recomputes the other side's coordinate with imul/idiv
**	    (truncating toward zero); dest clipping overrides source clipping.
**	  * Both step divisions zero EDX first, so the dividend is unsigned.
**
**	Divergence: where the assembly's idiv would fault (#DE) -- only possible
**	with nonsensical sizes -- this draws nothing instead of crashing. It
**	preserved EAX (USES eax), so its "return value" was the caller's EAX; this
**	returns 0.
*/
int WWPort_Linear_Scale_To_Linear(WWPortView const & src, WWPortView const & dst, int src_x, int src_y,
	int dst_x, int dst_y, int src_width, int src_height, int dst_width, int dst_height,
	int trans, unsigned char const * remap)
{
	if (dst_width == 0 || dst_height == 0 || src_width == 0 || src_height == 0) return 0;

	int32_t sx0 = src_x, sy0 = src_y;
	int32_t sx1 = (int32_t)((uint32_t)src_x + (uint32_t)src_width);
	int32_t sy1 = (int32_t)((uint32_t)src_y + (uint32_t)src_height);
	int32_t dx0 = dst_x, dy0 = dst_y;
	int32_t dx1 = (int32_t)((uint32_t)dst_x + (uint32_t)dst_width);
	int32_t dy1 = (int32_t)((uint32_t)dst_y + (uint32_t)dst_height);
	int32_t q;

	unsigned c0 = WWPort_Outcode(sx0, sy0, src.Width, src.Height);
	unsigned c1 = WWPort_Outcode(sx1, sy1, src.Width, src.Height);
	if (c0 & c1) return 0;
	if (c0 & 8) {
		sx0 = 0;
		if (!muldiv((int32_t)(0u - (uint32_t)src_x), dst_width, src_width, q)) return 0;
		dx0 = (int32_t)((uint32_t)q + (uint32_t)dst_x);
	}
	if (c0 & 2) {
		sy0 = 0;
		if (!muldiv((int32_t)(0u - (uint32_t)src_y), dst_height, src_height, q)) return 0;
		dy0 = (int32_t)((uint32_t)q + (uint32_t)dst_y);
	}
	if (c1 & 4) {
		sx1 = src.Width;
		if (!muldiv((int32_t)((uint32_t)src.Width - (uint32_t)src_x), dst_width, src_width, q)) return 0;
		dx1 = (int32_t)((uint32_t)q + (uint32_t)dst_x);
	}
	if (c1 & 1) {
		sy1 = src.Height;
		if (!muldiv((int32_t)((uint32_t)src.Height - (uint32_t)src_y), dst_height, src_height, q)) return 0;
		dy1 = (int32_t)((uint32_t)q + (uint32_t)dst_y);
	}

	c0 = WWPort_Outcode(dx0, dy0, dst.Width, dst.Height);
	c1 = WWPort_Outcode(dx1, dy1, dst.Width, dst.Height);
	if (c0 & c1) return 0;
	if (c0 & 8) {
		dx0 = 0;
		if (!muldiv((int32_t)(0u - (uint32_t)dst_x), src_width, dst_width, q)) return 0;
		sx0 = (int32_t)((uint32_t)q + (uint32_t)src_x);
	}
	if (c0 & 2) {
		dy0 = 0;
		if (!muldiv((int32_t)(0u - (uint32_t)dst_y), src_height, dst_height, q)) return 0;
		sy0 = (int32_t)((uint32_t)q + (uint32_t)src_y);
	}
	if (c1 & 4) {
		dx1 = dst.Width;
		if (!muldiv((int32_t)((uint32_t)dst.Width - (uint32_t)dst_x), src_width, dst_width, q)) return 0;
		sx1 = (int32_t)((uint32_t)q + (uint32_t)src_x);
	}
	if (c1 & 1) {
		dy1 = dst.Height;
		if (!muldiv((int32_t)((uint32_t)dst.Height - (uint32_t)dst_y), src_height, dst_height, q)) return 0;
		sy1 = (int32_t)((uint32_t)q + (uint32_t)src_y);
	}
	(void)sx1; (void)sy1;

	uint32_t const sstride = (uint32_t)src.Stride();
	uint32_t const dstride = (uint32_t)dst.Stride();
	unsigned char const * srow = src.Offset + (int32_t)((uint32_t)sx0 + sstride * (uint32_t)sy0);
	unsigned char * drow = dst.Offset + (int32_t)((uint32_t)dx0 + dstride * (uint32_t)dy0);

	int32_t ystep, yfrac, xq, unused;
	if (!udiv_by_signed((uint32_t)src_height, dst_height, ystep, yfrac)) return 0;
	int32_t const dy_intr = (int32_t)((uint32_t)ystep * sstride);
	uint32_t dy_acc = 0u - (uint32_t)dst_height;
	if (!udiv_by_signed((uint32_t)src_width << 16, dst_width, xq, unused)) return 0;
	uint32_t const xint = (uint32_t)xq >> 16;
	uint32_t const xfrac = (uint32_t)xq << 16;

	if (dy1 <= dy0) return 0;
	if (dx1 <= dx0) return 0;
	uint32_t rows = (uint32_t)dy1 - (uint32_t)dy0;
	uint32_t const width = (uint32_t)dx1 - (uint32_t)dx0;

	do {
		unsigned char const * s = srow;
		unsigned char * d = drow;
		uint32_t acc = 0;
		for (uint32_t i = 0; i < width; i++) {
			unsigned char pixel = *s;
			if (!trans || pixel) {
				*d = remap ? remap[pixel] : pixel;
			}
			uint32_t sum = acc + xfrac;
			s += xint + (sum < acc);
			acc = sum;
			d++;
		}
		drow += dstride;
		srow += dy_intr;
		int64_t b = (int64_t)(int32_t)dy_acc + yfrac;		// `add / jle`: the true sum
		uint32_t nb = dy_acc + (uint32_t)yfrac;
		if (b > 0) {
			srow += sstride;
			nb -= (uint32_t)dst_height;
		}
		dy_acc = nb;
	} while (--rows);
	return 0;
}

extern "C" BOOL __cdecl Linear_Scale_To_Linear(void * thisptr, void * dest, int src_x, int src_y, int dst_x, int dst_y,
	int src_width, int src_height, int dst_width, int dst_height, BOOL trans, char * remap)
{
	return WWPort_Linear_Scale_To_Linear(WWPort_View_Of((GraphicViewPortClass *)thisptr),
		WWPort_View_Of((GraphicViewPortClass *)dest), src_x, src_y, dst_x, dst_y,
		src_width, src_height, dst_width, dst_height, trans, (unsigned char const *)remap);
}
