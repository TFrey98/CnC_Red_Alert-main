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
**	DRAWLINE.CPP -- PORT-CREATED. C translation of DRAWLINE.ASM
**	(Buffer_Draw_Line). Verified against the original assembly:
**	port/tests/asm_drawbuff.cpp.
**
**	Kept close to the register level on purpose. Which pixels a line lights
**	depends on details a "clean" rewrite would lose:
**
**	  * The clip loop (Cohen-Sutherland) swaps the two end points every time
**	    round, so the end the line is DRAWN FROM depends on how many clip passes
**	    ran -- and Bresenham is not symmetric.
**	  * Clip points are computed with imul/idiv: a 64-bit product and a quotient
**	    truncated toward zero.
**	  * The error-term tests are `sub / jge`, a signed comparison of the old
**	    value with the subtrahend (overflow included); the start term is a
**	    LOGICAL shift right.
*/

#include "gbuffer.h"
#include "ww_view.h"
#include <string.h>

namespace {

struct LineClip {
	int32_t min_x, max_x, min_y, max_y;

	/* ??set_bits: 1 above, 2 below, 4 left, 8 right (signed compares). */
	uint32_t bits(int32_t x, int32_t y) const
	{
		uint32_t c = 0;
		if (y < min_y) c |= 1;
		if (y > max_y) c |= 2;
		if (x < min_x) c |= 4;
		if (x > max_x) c |= 8;
		return c;
	}
};

/* imul / idiv: 64-bit product, quotient truncated toward zero. */
inline uint32_t muldiv(uint32_t a, uint32_t b, uint32_t d)
{
	return (uint32_t)(int32_t)((int64_t)(int32_t)a * (int32_t)b / (int32_t)d);
}

/*
**	??clip_vert: move (eax,ebx) along the line to ebx = esi.
*/
inline void clip_vert(uint32_t & eax, uint32_t & ebx, uint32_t ecx, uint32_t edx, uint32_t esi)
{
	uint32_t q = muldiv(ecx - eax, esi - ebx, edx - ebx);
	eax = q + eax;
	ebx = esi;
}

/*
**	??clip_horiz: move (eax,ebx) along the line to eax = esi.
*/
inline void clip_horiz(uint32_t & eax, uint32_t & ebx, uint32_t ecx, uint32_t edx, uint32_t esi)
{
	uint32_t q = muldiv(esi - eax, edx - ebx, ecx - eax);
	ebx += q;
	eax = esi;
}

/*
**	??clip_tbl: returns true (carry set) when it clipped something.
*/
bool clip(uint32_t code, LineClip const & c, uint32_t & eax, uint32_t & ebx, uint32_t & ecx, uint32_t & edx)
{
	static unsigned char const table[16] = {
		0, 'u', 'd', 0,   'l', 'l', 'd', 0,   'r', 'u', 'r', 0,   0, 0, 0, 0
	};
	switch (table[code & 15]) {
		case 'u':
			clip_vert(eax, ebx, ecx, edx, (uint32_t)c.min_y);
			return true;
		case 'd': {
			uint32_t nb = 0u - ebx, nd = 0u - edx;
			clip_vert(eax, nb, ecx, nd, 0u - (uint32_t)c.max_y);
			ebx = 0u - nb;
			return true;
		}
		case 'l':
			clip_horiz(eax, ebx, ecx, edx, (uint32_t)c.min_x);
			return true;
		case 'r': {
			uint32_t na = 0u - eax, nc = 0u - ecx;
			clip_horiz(na, ebx, nc, edx, 0u - (uint32_t)c.max_x);
			eax = 0u - na;
			return true;
		}
		default:
			return false;
	}
}

}

void WWPort_Buffer_Draw_Line(WWPortView const & v, int sx, int sy, int dx, int dy, unsigned char color)
{
	LineClip c = {0, v.Width - 1, 0, v.Height - 1};
	uint32_t const bpr = (uint32_t)v.Stride();
	uint32_t eax = (uint32_t)sx, ebx = (uint32_t)sy, ecx = (uint32_t)dx, edx = (uint32_t)dy;

	if (c.bits((int32_t)eax, (int32_t)ebx) | c.bits((int32_t)ecx, (int32_t)edx)) {
		for (;;) {
			uint32_t edi = c.bits((int32_t)eax, (int32_t)ebx);
			uint32_t t;
			t = eax; eax = ecx; ecx = t;
			t = ebx; ebx = edx; edx = t;
			uint32_t esi = c.bits((int32_t)eax, (int32_t)ebx);
			if ((edi | esi) == 0) break;
			if (edi & esi) return;
			if (clip(esi, c, eax, ebx, ecx, edx)) continue;
			t = eax; eax = ecx; ecx = t;
			t = ebx; ebx = edx; edx = t;
			clip(edi, c, eax, ebx, ecx, edx);
		}
	}

	/* ??draw_it: (eax,ebx) -> (ecx,edx) */
	unsigned char * p;
	if (edx == ebx) {									// ??hline
		if (!((int32_t)eax < (int32_t)ecx)) {uint32_t t = eax; eax = ecx; ecx = t;}
		uint32_t count = ecx - eax + 1;
		p = v.Offset + (uint32_t)(bpr * ebx) + eax;
		memset(p, color, count);
		return;
	}
	if ((int32_t)edx > (int32_t)ebx) {
		edx = edx - ebx;
	} else {
		edx = ebx - edx;
		uint32_t t = eax; eax = ecx; ecx = t;
		ebx = ebx - edx;
	}
	p = v.Offset + (uint32_t)(bpr * ebx);			// start of the top line

	int32_t step = 1;
	if (ecx == eax) {									// ??vline
		p += eax;
		for (uint32_t n = edx + 1; n; n--) {
			*p = color;
			p += bpr;
		}
		return;
	}
	if ((int32_t)ecx > (int32_t)eax) {
		ecx = ecx - eax;
	} else {
		ecx = eax - ecx;
		step = -1;
	}

	if ((int32_t)ecx >= (int32_t)edx) {				// ??horiz: x major
		uint32_t const major = ecx, minor = edx;
		uint32_t accum = major >> 1;
		int32_t n = (int32_t)major;
		p += eax;
		for (;;) {
			*p = color;
			if (--n < 0) return;
			p += step;
			bool ge = (int32_t)accum >= (int32_t)minor;
			accum -= minor;
			if (!ge) {
				accum += major;
				p += bpr;
			}
		}
	} else {												// ??vert: y major
		uint32_t const major = edx, minor = ecx;
		uint32_t accum = major >> 1;
		int32_t n = (int32_t)major;
		p += eax;
		for (;;) {
			*p = color;
			if (--n < 0) return;
			p += bpr;
			bool ge = (int32_t)accum >= (int32_t)minor;
			accum -= minor;
			if (!ge) {
				accum += major;
				p += step;
			}
		}
	}
}

extern "C" void __cdecl Buffer_Draw_Line(void * thisptr, int sx, int sy, int dx, int dy, unsigned char color)
{
	WWPort_Buffer_Draw_Line(WWPort_View_Of((GraphicViewPortClass *)thisptr), sx, sy, dx, dy, color);
}
