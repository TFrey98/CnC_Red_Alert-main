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
**	WWMOUSE.CPP -- PORT-CREATED. C translation of WWMOUSE.ASM: the software
**	mouse cursor -- saving/restoring what is under it (Mouse_Shadow_Buffer),
**	drawing it (Draw_Mouse), and decoding a cursor shape (ASM_Set_Mouse_Cursor).
**	Verified against the original assembly: port/tests/asm_misc.cpp.
**
**	The assembly read WWMouseClass through a 32-bit struct overlay (MOUSE.INC);
**	these entry points read its fields by name (WWPortMouseAccess, a friend)
**	and the work is done on plain values, which the tests drive directly.
*/

#include "wwstd.h"
#include "gbuffer.h"
#include "mouse.h"
#include "shape.h"
#include "ww_view.h"
#include <string.h>

extern "C" unsigned long __cdecl LCW_Uncompress(void * source, void * dest, unsigned long length);

/*
**	The cursor's clip, as the assembly computed it: the drawing code's
**	outcodes for the cursor rectangle (x - hotx, y - hoty, width x height);
**	returns false when nothing is visible. `bx`/`by` are where in the cursor
**	(or its save buffer) the visible part starts.
*/
static bool cursor_clip(WWPortView const & v, int32_t x0, int32_t y0, int32_t w, int32_t h,
	int32_t & cx0, int32_t & cy0, int32_t & cx1, int32_t & cy1, int32_t & bx, int32_t & by)
{
	cx0 = x0; cy0 = y0;
	cx1 = (int32_t)((uint32_t)x0 + (uint32_t)w);
	cy1 = (int32_t)((uint32_t)y0 + (uint32_t)h);
	bx = 0; by = 0;
	unsigned c0 = WWPort_Outcode(cx0, cy0, v.Width, v.Height);
	unsigned c1 = WWPort_Outcode(cx1, cy1, v.Width, v.Height);
	if (c0 & c1) return false;
	if (c0 & 8) {bx = -cx0; cx0 = 0;}
	if (c0 & 2) {by = -cy0; cy0 = 0;}
	if (c1 & 4) cx1 = v.Width;
	if (c1 & 1) cy1 = v.Height;
	return cx1 > cx0 && cy1 > cy0;			// `sub / jle`, signed
}

/*
**	store == 1: copy the screen under the cursor into `buffer` (cursor-width
**	lines); anything else: copy `buffer` back to the screen.
*/
void WWPort_Mouse_Shadow_Buffer(int cursor_width, int cursor_height, WWPortView const & v, unsigned char * buffer,
	int x, int y, int hotx, int hoty, int store)
{
	int32_t x0, y0, x1, y1, bx, by;
	if (!cursor_clip(v, x - hotx, y - hoty, cursor_width, cursor_height, x0, y0, x1, y1, bx, by)) return;
	int32_t const w = x1 - x0, rows = y1 - y0;
	unsigned char * scr = v.Offset + (int32_t)((uint32_t)y0 * (uint32_t)v.Stride() + (uint32_t)x0);
	unsigned char * buf = buffer + (int32_t)((uint32_t)by * (uint32_t)cursor_width) + bx;
	for (int32_t r = 0; r < rows; r++) {
		if (store == 1) memcpy(buf, scr, (uint32_t)w); else memcpy(scr, buf, (uint32_t)w);
		scr += v.Stride();
		buf += cursor_width;
	}
}

/*
**	Draw the cursor's non-zero pixels. The assembly's counters are 8-bit
**	(a 256-wide or -tall cursor would wrap); cursors are far smaller.
*/
void WWPort_Draw_Mouse(unsigned char const * cursor, int cursor_width, int cursor_height, int xhot, int yhot,
	WWPortView const & v, int x, int y)
{
	int32_t x0, y0, x1, y1, bx, by;
	if (!cursor_clip(v, x - xhot, y - yhot, cursor_width, cursor_height, x0, y0, x1, y1, bx, by)) return;
	unsigned char const w8 = (unsigned char)(x1 - x0);
	unsigned char rows8 = (unsigned char)(y1 - y0);
	int32_t const w = x1 - x0;
	unsigned char * d = v.Offset + (int32_t)((uint32_t)y0 * (uint32_t)v.Stride() + (uint32_t)x0);
	unsigned char const * s = cursor + (int32_t)((uint32_t)by * (uint32_t)cursor_width) + bx;
	do {
		unsigned char n = w8;
		do {
			if (*s) *d = *s;
			s++;
			d++;
		} while (--n);
		s += cursor_width - w;
		d += v.Stride() - w;
	} while (--rows8);
}

struct WWPortMouseFields {
	unsigned char *	MouseCursor;
	int					MouseXHot, MouseYHot, CursorWidth, CursorHeight, MaxWidth, MaxHeight;
	void *				PrevCursor;
};

/*
**	Decode a cursor shape into MouseCursor. Shapes are a 10-byte header
**	(Shape_Type), a 16-entry colour table for 16-colour ("compact") ones, and
**	run-length data: non-zero bytes are pixels (through the colour table when
**	compact), 0 n is n transparent pixels. LCW-compressed shapes are first
**	unpacked into _ShapeBuffer behind a copy of their header. A shape wider or
**	taller than the cursor buffer is refused. Either way the previous cursor
**	pointer is returned and this one remembered.
*/
void * WWPort_Set_Mouse_Cursor(WWPortMouseFields & m, int xhotspot, int yhotspot, void * cursor)
{
	unsigned char const * shape = (unsigned char const *)cursor;
	int32_t const datasize = Get_Shape_Uncomp_Size(cursor);
	int32_t const width = Get_Shape_Width(cursor);
	if (width <= m.MaxWidth) {
		int32_t const height = Get_Shape_Original_Height(cursor);
		if (height <= m.MaxHeight) {
			uint16_t stype = (uint16_t)(shape[0] | shape[1] << 8);
			unsigned char const * s = shape;
			if (!(stype & MAKESHAPE_NOCOMP)) {
				unsigned char * u = (unsigned char *)_ShapeBuffer;
				stype |= MAKESHAPE_NOCOMP;
				u[0] = (unsigned char)stype;
				u[1] = (unsigned char)(stype >> 8);
				int const header = (stype & MAKESHAPE_COMPACT) ? 24 : 8;
				memcpy(u + 2, shape + 2, header);
				LCW_Uncompress((void *)(shape + 2 + header), u + 2 + header, (unsigned long)datasize);
				s = u;
			}
			unsigned char const * colors = s + 10;
			bool const compact = (stype & MAKESHAPE_COMPACT) != 0;
			s += compact ? 26 : 10;
			unsigned char * d = m.MouseCursor;
			int32_t left = (int32_t)((uint32_t)width * (uint32_t)height);
			while (left != 0) {
				unsigned char b = *s++;
				if (b) {
					*d++ = compact ? colors[b] : b;
					left--;
				} else {
					unsigned char run = *s++;
					left -= run;
					memset(d, 0, run);
					d += run;
				}
			}
			m.MouseXHot = xhotspot;
			m.MouseYHot = yhotspot;
			m.CursorHeight = height;
			m.CursorWidth = width;
		}
	}
	void * prev = m.PrevCursor;
	m.PrevCursor = cursor;
	return prev;
}

/*
**	WWMouseClass's private fields, by name.
*/
struct WWPortMouseAccess {
	static WWPortMouseFields get(WWMouseClass * m)
	{
		WWPortMouseFields f = {(unsigned char *)m->MouseCursor, m->MouseXHot, m->MouseYHot, m->CursorWidth,
			m->CursorHeight, m->MaxWidth, m->MaxHeight, m->PrevCursor};
		return f;
	}
	static void put(WWMouseClass * m, WWPortMouseFields const & f)
	{
		m->MouseXHot = f.MouseXHot; m->MouseYHot = f.MouseYHot;
		m->CursorWidth = f.CursorWidth; m->CursorHeight = f.CursorHeight;
		m->PrevCursor = (char *)f.PrevCursor;
	}
};

extern "C" void __cdecl Mouse_Shadow_Buffer(void * thisptr, GraphicViewPortClass * srcdst, void * buffer, int x, int y,
	int hotx, int hoty, int store)
{
	WWPortMouseFields f = WWPortMouseAccess::get((WWMouseClass *)thisptr);
	WWPort_Mouse_Shadow_Buffer(f.CursorWidth, f.CursorHeight, WWPort_View_Of(srcdst), (unsigned char *)buffer,
		x, y, hotx, hoty, store);
}

extern "C" void __cdecl Draw_Mouse(void * thisptr, GraphicViewPortClass * srcdst, int x, int y)
{
	WWPortMouseFields f = WWPortMouseAccess::get((WWMouseClass *)thisptr);
	WWPort_Draw_Mouse(f.MouseCursor, f.CursorWidth, f.CursorHeight, f.MouseXHot, f.MouseYHot,
		WWPort_View_Of(srcdst), x, y);
}

extern "C" void * __cdecl ASM_Set_Mouse_Cursor(void * thisptr, int hotspotx, int hotspoty, VOID * cursor)
{
	WWPortMouseFields f = WWPortMouseAccess::get((WWMouseClass *)thisptr);
	void * prev = WWPort_Set_Mouse_Cursor(f, hotspotx, hotspoty, cursor);
	WWPortMouseAccess::put((WWMouseClass *)thisptr, f);
	return prev;
}
