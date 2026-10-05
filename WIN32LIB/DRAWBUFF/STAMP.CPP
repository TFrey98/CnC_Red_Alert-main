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
**	STAMP.CPP -- PORT-CREATED. C translation of STAMP.ASM: Init_Stamps,
**	Buffer_Draw_Stamp and Buffer_Draw_Stamp_Clip, which draw terrain icons
**	("stamps") from an icon set. Verified against the original assembly:
**	port/tests/asm_drawbuff.cpp.
*/

#include "gbuffer.h"
#include "tile.h"
#include "ww_view.h"
#include "stampvar.h"
#include <string.h>

/*
**	The icon set last seen, decoded once (STAMP.ASM's DATASEG). STMPCACH.CPP
**	reads these too.
*/
extern "C" {
void const *			LastIconset = NULL;
unsigned char const *	StampPtr = NULL;
unsigned char const *	IsTrans = NULL;
unsigned char const *	MapPtr = NULL;
int32_t					IconWidth = 0;
int32_t					IconHeight = 0;
int32_t					IconSize = 0;
int32_t					IconCount = 0;
}

/*
**	MapPtr is the header's Map offset ADDED to the set's address, so it is
**	never NULL once a set is loaded -- every icon number goes through the map.
*/
extern "C" void Init_Stamps(void const * icondata)
{
	if (icondata == NULL || icondata == LastIconset) return;
	IControl_Type const * h = (IControl_Type const *)icondata;
	unsigned char const * base = (unsigned char const *)icondata;
	LastIconset = icondata;
	IconCount = (uint16_t)h->Count;
	IconWidth = (uint16_t)h->Width;
	IconHeight = (uint16_t)h->Height;
	IconSize = (int32_t)((uint32_t)IconWidth * (uint32_t)IconHeight);
	MapPtr = base + h->Map;
	StampPtr = base + h->Icons;
	IsTrans = base + h->TransFlag;
}

/*
**	`mov bl,[MapPtr+icon]`: only the LOW BYTE of the icon number is replaced.
**	Returns false when the result is not below IconCount (unsigned compare).
*/
bool WWPort_Stamp_Index(int & icon)
{
	uint32_t i = (uint32_t)icon;
	if (MapPtr) i = (i & ~0xFFu) | MapPtr[(int32_t)i];
	if (i >= (uint32_t)IconCount) return false;
	icon = (int)i;
	return true;
}

/*
**	Unclipped. Faithful to the assembly:
**	  * With a remap table, colour 0 AFTER remapping is transparent.
**	  * Otherwise the icon's IsTrans flag picks a transparent copy (colour 0
**	    skipped) or an opaque one; the opaque copy moves whole dwords four lines
**	    at a time, so it draws (IconHeight & ~3) lines of (IconWidth & ~3)
**	    bytes, and the source advances only by what was copied. Icons are
**	    24 x 24, where this is exact.
**	Divergence: an opaque icon under 4 lines high made the assembly's line
**	counter wrap (2^32 iterations); this draws nothing.
*/
void WWPort_Buffer_Draw_Stamp(WWPortView const & v, void const * icondata, int icon, int x, int y, unsigned char const * remap)
{
	if (icondata == NULL) return;
	if (LastIconset != icondata) Init_Stamps(icondata);
	if (!WWPort_Stamp_Index(icon)) return;

	uint32_t const stride = (uint32_t)v.Stride();
	unsigned char * d = v.Offset + (int32_t)(stride * (uint32_t)y + (uint32_t)x);
	int32_t const modulo = (int32_t)(stride - (uint32_t)IconWidth);
	unsigned char const * s = StampPtr + (uint32_t)icon * (uint32_t)IconSize;

	if (remap) {
		for (int32_t r = 0; r < IconHeight; r++) {
			for (int32_t i = 0; i < IconWidth; i++, s++, d++) {
				unsigned char c = remap[*s];
				if (c) *d = c;
			}
			d += modulo;
		}
	} else if (IsTrans[icon]) {
		for (int32_t r = 0; r < IconHeight; r++) {
			for (int32_t i = 0; i < IconWidth; i++, s++, d++) {
				if (*s) *d = *s;
			}
			d += modulo;
		}
	} else {
		uint32_t const bytes = (uint32_t)IconWidth & ~3u;
		for (uint32_t r = 0; r < ((uint32_t)IconHeight >> 2) * 4; r++) {
			memmove(d, s, bytes);
			s += bytes;
			d += bytes + modulo;
		}
	}
}

/*
**	Clipped to a window: (min_x,min_y) is its corner IN the view and
**	(max_x,max_y) its SIZE; x,y are relative to the window. Faithful to the
**	assembly, including transparency after remapping (as above).
**	Divergence: a window of zero or negative size could leave a line width or
**	count of 0 or less, which the assembly's `loop` turned into 2^32
**	iterations; this draws nothing.
*/
void WWPort_Buffer_Draw_Stamp_Clip(WWPortView const & v, void const * icondata, int icon, int x, int y,
	unsigned char const * remap, int min_x, int min_y, int max_x, int max_y)
{
	if (icondata == NULL) return;
	if (LastIconset != icondata) Init_Stamps(icondata);
	if (!WWPort_Stamp_Index(icon)) return;

	int32_t rows = IconHeight;
	int32_t iwidth = IconWidth;
	unsigned char const * s = StampPtr + (uint32_t)icon * (uint32_t)IconSize;

	max_x += min_x; x += min_x;
	max_y += min_y; y += min_y;
	if (x >= max_x) return;
	if (y >= max_y) return;
	if (y + IconHeight <= min_y) return;
	if (x + IconWidth <= min_x) return;

	if (x < min_x) {
		s += min_x - x;
		iwidth -= min_x - x;
		x = min_x;
	}
	int32_t skip = IconWidth - iwidth;
	if (x + iwidth > max_x) {
		int32_t old = iwidth;
		iwidth = max_x - x;
		skip += old - iwidth;
	}
	if (min_y > y) {
		rows -= min_y - y;
		s += (uint32_t)(min_y - y) * (uint32_t)IconWidth;
		y = min_y;
	}
	if (y + rows > max_y) rows = max_y - y;
	if (iwidth <= 0 || rows <= 0) return;

	uint32_t const stride = (uint32_t)v.Stride();
	unsigned char * d = v.Offset + (int32_t)(stride * (uint32_t)y + (uint32_t)x);
	int32_t const modulo = (int32_t)(stride - (uint32_t)iwidth);

	for (int32_t r = 0; r < rows; r++) {
		if (remap) {
			for (int32_t i = 0; i < iwidth; i++, s++, d++) {
				unsigned char c = remap[*s];
				if (c) *d = c;
			}
		} else if (IsTrans[icon]) {
			for (int32_t i = 0; i < iwidth; i++, s++, d++) {
				if (*s) *d = *s;
			}
		} else {
			memmove(d, s, (uint32_t)iwidth);
			s += iwidth;
			d += iwidth;
		}
		d += modulo;
		s += skip;
	}
}

extern "C" void __cdecl Buffer_Draw_Stamp(void const * thisptr, void const * icondata, int icon, int x_pixel, int y_pixel, void const * remap)
{
	WWPort_Buffer_Draw_Stamp(WWPort_View_Of((GraphicViewPortClass const *)thisptr), icondata, icon, x_pixel, y_pixel,
		(unsigned char const *)remap);
}

extern "C" void __cdecl Buffer_Draw_Stamp_Clip(void const * thisptr, void const * icondata, int icon, int x_pixel, int y_pixel,
	void const * remap, int min_x, int min_y, int max_x, int max_y)
{
	WWPort_Buffer_Draw_Stamp_Clip(WWPort_View_Of((GraphicViewPortClass const *)thisptr), icondata, icon, x_pixel, y_pixel,
		(unsigned char const *)remap, min_x, min_y, max_x, max_y);
}
