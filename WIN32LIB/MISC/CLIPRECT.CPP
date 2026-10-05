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
**	CLIPRECT.CPP -- PORT-CREATED. C translation of CLIPRECT.ASM (Clip_Rect,
**	Confine_Rect). Verified against the original assembly: port/tests/asm_misc.cpp.
*/

#include "wwstd.h"
#include "misc.h"
#include "ww_view.h"

/*
**	Clip a rectangle (in place) to 0..width x 0..height. Returns -1 if it lies
**	wholly outside (or clips to nothing on the right/bottom), 0 if it needed no
**	clipping, 1 if it was clipped. The outcode test is the drawing code's
**	(WWPort_Outcode); the right/bottom checks are `sub / jle`, a signed
**	comparison of the operands.
*/
extern "C" int __cdecl Clip_Rect(int * x, int * y, int * w, int * h, int width, int height)
{
	int32_t x0 = *x, y0 = *y;
	int32_t x1 = (int32_t)((uint32_t)x0 + (uint32_t)*w);
	int32_t y1 = (int32_t)((uint32_t)y0 + (uint32_t)*h);
	unsigned c0 = WWPort_Outcode(x0, y0, width, height);
	unsigned c1 = WWPort_Outcode(x1, y1, width, height);
	if (c0 & c1) return -1;
	if ((c0 | c1) == 0) return 0;
	if (c0 & 8) {int32_t t = *x; *x = 0; *w = (int32_t)((uint32_t)*w + (uint32_t)t);}
	if (c0 & 2) {int32_t t = *y; *y = 0; *h = (int32_t)((uint32_t)*h + (uint32_t)t);}
	if (c1 & 4) {
		*w = (int32_t)((uint32_t)width - (uint32_t)*x);
		if (width <= *x) return -1;
	}
	if (c1 & 1) {
		*h = (int32_t)((uint32_t)height - (uint32_t)*y);
		if (height <= *y) return -1;
	}
	return 1;
}

/*
**	Move a w x h rectangle (in place) so it lies within 0..width x 0..height
**	where it can: a side that already fits is untouched; one that does not is
**	set to 0 if it started at or left of 0, otherwise shifted back by its
**	overhang. Returns 1 if anything moved. Faithful to the assembly's test:
**	the axis "fits" when -x and x+w-width-1 are BOTH negative (sign bit of
**	their AND).
*/
static int confine(int * pos, int size, int limit)
{
	uint32_t neg = 0u - (uint32_t)*pos;
	uint32_t over = (uint32_t)size + (uint32_t)*pos - (uint32_t)limit - 1u;
	if ((int32_t)(neg & over) < 0) return 0;
	if ((int32_t)neg < 0) {
		*pos = (int32_t)((uint32_t)*pos - (over + 1u));
	} else {
		*pos = 0;
	}
	return 1;
}

extern "C" int __cdecl Confine_Rect(int * x, int * y, int w, int h, int width, int height)
{
	int moved = confine(x, w, width);
	if (confine(y, h, height)) moved = 1;
	return moved;
}
