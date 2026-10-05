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
**	2TXTPRNT.CPP -- PORT-CREATED. C translation of 2TXTPRNT.ASM: Buffer_Print,
**	the 4-bit proportional font printer, and the colour table it owns.
**	Verified against the original assembly: port/tests/asm_game.cpp.
*/

#include <gbuffer.h>
#include <font.h>
#include "ww_view.h"
#include <string.h>

/*
**	Font colour translation: entry n is colour n of a glyph's low nibble,
**	entry n * 16 the same colour for its high nibble. Buffer_Print sets 0
**	(background) and 1/16 (foreground); Set_Font_Palette_Range sets the rest.
**	The assembly's data segment defined 241 bytes -- entries 0..240, which
**	covers the highest high-nibble index (0xF0); the rest are 0 here.
*/
extern "C" unsigned char ColorXlat[256] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x0F,
};

extern "C" void * __cdecl Get_Font_Palette_Ptr(void)
{
	return ColorXlat;
}

static inline unsigned rd16(unsigned char const * p) {return (unsigned)p[0] | ((unsigned)p[1] << 8);}

/* FONTINFOBLOCK etc. are font.h's, the same values as the assembly's EQUs. */

/*
**	Print a string in the current font (FontPtr) at (x,y). Returns where the
**	next character would go, or NULL when the text ran off the bottom (or there
**	is no font). Faithful to the assembly, including what a reader might not
**	expect:
**
**	  * Glyph data is 4 bits a pixel, low nibble first; each row starts on a
**	    fresh byte, so an odd-width glyph's last high nibble is unused.
**	  * Colour 0 after translation is transparent; a glyph's blank rows above
**	    and below are filled with the background colour unless it is 0.
**	  * Per-glyph counters are 8-bit, so a zero-width glyph draws 256 pixels a
**	    row, and a glyph taller than the font's maxheight wraps its bottom
**	    blank count. No shipped font has either.
**	  * LF (10) returns to column 0; CR (13) and a wrap forced by the right edge
**	    return to the starting x.
*/
unsigned char * WWPort_Buffer_Print(WWPortView const & v, char const * string, int x, int y, int fcolor, int bcolor)
{
	uint32_t const bw = (uint32_t)v.Stride();
	unsigned char * curline = v.Offset + (int32_t)(bw * (uint32_t)y);
	unsigned char * startdraw = curline + x;
	int32_t const original_x = x;
	unsigned char const * font = (unsigned char const *)FontPtr;
	if (font == NULL) return NULL;

	unsigned char const * offsets = font + rd16(font + FONTOFFSETBLOCK);
	unsigned char const * widths = font + rd16(font + FONTWIDTHBLOCK);
	unsigned char const * heights = font + rd16(font + FONTHEIGHTBLOCK);
	unsigned char const maxheight = (font + rd16(font + FONTINFOBLOCK))[FONTINFOMAXHEIGHT];

	if ((int32_t)((uint32_t)maxheight + (uint32_t)y) > v.Height) return NULL;
	y += maxheight;

	ColorXlat[1] = (unsigned char)fcolor;
	ColorXlat[16] = (unsigned char)fcolor;
	ColorXlat[0] = (unsigned char)bcolor;

	unsigned char const * s = (unsigned char const *)string;
	for (;;) {
		unsigned char c = *s;
		if (c == 0) return startdraw;
		unsigned char * d = startdraw;
		s++;

		unsigned char feed = c;
		if (c != 10 && c != 13) {
			uint32_t const width = widths[c];
			int32_t nx = (int32_t)((uint32_t)x + width + (uint32_t)FontXSpacing);
			startdraw += (int32_t)(width + (uint32_t)FontXSpacing);
			if (nx > v.Width) {
				s--;												// print this character on the next line
				feed = 0;
			} else {
				x = nx;
				int32_t const nextdraw = (int32_t)(bw - width);
				unsigned char const * src = font + rd16(offsets + 2 * c);
				unsigned char const top = heights[2 * c];
				unsigned char const charheight = heights[2 * c + 1];
				unsigned char const bottom = (unsigned char)(maxheight - (unsigned char)(charheight + top));
				unsigned char const dl = (unsigned char)width;

				if (top) {
					unsigned char bg = ColorXlat[0];
					if (bg) {
						for (unsigned char rows = top; rows; rows--) {
							unsigned char n = dl;
							do {*d++ = bg;} while (--n);
							d += nextdraw;
						}
					} else {
						d += (int32_t)((uint32_t)top * bw);
					}
				}
				if (charheight == 0) continue;
				for (unsigned rows = charheight; rows; rows--) {
					unsigned char n = dl;
					for (;;) {
						unsigned char b = *src++;
						unsigned char px = ColorXlat[b & 0x0F];
						if (px) *d = px;
						d++;
						if (--n == 0) break;
						px = ColorXlat[b & 0xF0];
						if (px) *d = px;
						d++;
						if (--n == 0) break;
					}
					d += nextdraw;
				}
				if (bottom == 0) continue;
				unsigned char bg = ColorXlat[0];
				if (bg == 0) continue;
				for (unsigned char rows = bottom; rows; rows--) {
					unsigned char n = dl;
					do {*d++ = bg;} while (--n);
					d += nextdraw;
				}
				continue;
			}
		}

		/* line feed: down one line of text */
		uint32_t const adv = (uint32_t)maxheight + (uint32_t)FontYSpacing;
		if ((int32_t)((uint32_t)y + adv) > v.Height) return NULL;
		curline += (int32_t)(bw * adv);
		y = (int32_t)((uint32_t)y + adv);
		x = (feed == 10) ? 0 : original_x;
		startdraw = curline + x;
	}
}

/*
**	The prototype's LONG is 32 bits, so the returned address is truncated here
**	as it never was on Win32 -- no caller uses it (GBUFFER.H's Print() passes
**	it back up; nothing in CODE/ reads that).
*/
extern "C" LONG __cdecl Buffer_Print(void * thisptr, const char * str, int x, int y, int fcolor, int bcolor)
{
	return (LONG)(intptr_t)WWPort_Buffer_Print(WWPort_View_Of((GraphicViewPortClass *)thisptr), str, x, y, fcolor, bcolor);
}
