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
**	WINASM.CPP -- PORT-CREATED. C translation of the parts of WINASM.ASM the
**	game uses: the palette-interpolating 2x scalers, Stop_Execution, and a
**	native replacement for ModeX_Blit. Verified against the original assembly:
**	port/tests/asm_game.cpp.
**
**	Not carried over, because nothing in the linked game calls them
**	(port/LINK-CENSUS.md): Asm_Create_Palette_Interpolation_Table (INTERPAL.CPP
**	has the C version it uses), _Int3, and the empty serial-modem entry points.
*/

#include "function.h"
#include "ww_view.h"
#include <string.h>

/*
**	NATIVE replacement for Set_Palette_Register_ (WINSTUB.CPP's colour cycling):
**	one VGA DAC entry, each component masked to 6 bits, written through the
**	same emulated DAC ports as the rest of the palette code (PRAGMAUX.CPP).
*/
extern "C" void Set_Palette_Register(int number, int red, int green, int blue)
{
	outportb(0x3C8, (unsigned char)number);
	outportb(0x3C9, (unsigned char)(red & 63));
	outportb(0x3C9, (unsigned char)(green & 63));
	outportb(0x3C9, (unsigned char)(blue & 63));
}

/*
**	The scroll-lock "break into the debugger" hook: the assembly was `nop / ret`.
*/
extern "C" void __cdecl Stop_Execution(void)
{
}

/*
**	PaletteInterpolationTable[b][a] is the colour between a and b
**	(INTERPAL.CPP); the assembly indexed it with the two pixels as one 16-bit
**	value, first pixel in the low byte.
*/
static inline unsigned char mid(unsigned char a, unsigned char b)
{
	return PaletteInterpolationTable[b][a];
}

/*
**	One source line, w pixels, into 2w output bytes:
**	  s0, mid(s0,s1), s1, mid(s1,s2), ... , s[w-2], mid(s[w-2],s[w-1]), s[w-1], 0
**	(the assembly's tail writes a 0 where the last in-between pixel would go).
**	Returns the source advanced past the pixels consumed: w for even w,
**	w - 1 for odd w, as the assembly's pointer arithmetic did.
**	Divergence: w of 2 or 3 made the assembly's pair counter wrap (2^32
**	iterations); this writes just the tail.
*/
static unsigned char const * interpolate_line(unsigned char const * s, unsigned char * d, uint32_t w)
{
	uint32_t pairs = (w - 2) >> 1;
	for (uint32_t i = 0; i < pairs; i++) {
		d[0] = s[0];
		d[1] = mid(s[0], s[1]);
		d[2] = s[1];
		d[3] = mid(s[1], s[2]);
		s += 2;
		d += 4;
	}
	d[0] = s[0];
	d[1] = mid(s[0], s[1]);
	d[2] = s[1];
	d[3] = 0;
	return s + 2;
}

/*
**	Copy type 0: each line interpolated across; lines dest_width apart.
*/
extern "C" void __cdecl Asm_Interpolate(unsigned char * src_ptr, unsigned char * dest_ptr, int source_height,
	int source_width, int dest_width)
{
	unsigned char const * s = src_ptr;
	unsigned char * d = dest_ptr;
	for (int line = source_height; line > 0; line--) {
		s = interpolate_line(s, d, (uint32_t)source_width);
		d += dest_width;
	}
}

/*
**	Copy type 1: each interpolated line drawn twice, dest_width / 2 apart (the
**	caller passes twice the destination pitch). The second copy is
**	source_width / 2 dwords, i.e. 2 * source_width bytes for an even width.
*/
extern "C" void __cdecl Asm_Interpolate_Line_Double(unsigned char * src_ptr, unsigned char * dest_ptr, int source_height,
	int source_width, int dest_width)
{
	unsigned char const * s = src_ptr;
	unsigned char * d = dest_ptr;
	uint32_t const half = (uint32_t)dest_width >> 1;
	uint32_t const bytes = ((uint32_t)source_width >> 1) * 4;
	for (int line = source_height; line > 0; line--) {
		s = interpolate_line(s, d, (uint32_t)source_width);
		memmove(d + half, d, bytes);
		d += 2 * half;
	}
}

/*
**	Copy type 2: lines interpolated across AND between: each interpolated line
**	is followed by the colour-between of it and the next.
**
**	Faithful to an over-read in the assembly: the final output line is
**	interpolated from the line AFTER the last source line (src + lines * w),
**	not the last one. With source_lines < 2 the assembly's line counter
**	wrapped; this does nothing then.
*/
extern "C" void __cdecl Asm_Interpolate_Line_Interpolate(unsigned char * src_ptr, unsigned char * dest_ptr, int source_lines,
	int source_width, int dest_width)
{
	static unsigned char top[640 * 4], bottom[640 * 4], between[640 * 4];
	if (source_lines < 2) return;
	uint32_t const w = (uint32_t)source_width;
	uint32_t const bytes = (w >> 1) * 4;
	uint32_t const half = (uint32_t)dest_width >> 1;
	unsigned char * next = top;
	unsigned char * last = bottom;
	unsigned char * d = dest_ptr;
	unsigned char const * s = src_ptr;

	interpolate_line(s, next, w);
	{unsigned char * t = next; next = last; last = t;}
	s += w;
	for (int lines = source_lines - 1; lines > 0; lines--) {
		interpolate_line(s, next, w);
		for (uint32_t i = 0; i < 2 * w; i++) between[i] = mid(last[i], next[i]);
		memcpy(d, last, bytes);
		d += half;
		memcpy(d, between, bytes);
		d += half;
		s += w;
		unsigned char * t = next; next = last; last = t;
	}
	interpolate_line(s, next, w);
	memcpy(d, next, bytes);
}

/*
**	NATIVE replacement. On a 320-wide display the game ran in VGA "mode X",
**	and this wrote the hidden page into the four VGA planes through the
**	sequencer's map-mask register -- hardware a Mac does not have. What it put
**	on screen is exactly the hidden page's 320 x 200 pixels (shown by running
**	the original under the emulator with the VGA planes modelled:
**	port/asmref, `modex_blit`), so here it is that copy, to the visible page.
*/
void WWPort_ModeX_Blit(WWPortView const & src, WWPortView const & dst)
{
	for (int y = 0; y < 200; y++) {
		memcpy(dst.Offset + (size_t)y * (uint32_t)dst.Stride(), src.Offset + (size_t)y * (uint32_t)(320 + src.XAdd + src.Pitch), 320);
	}
}

extern "C" void ModeX_Blit(GraphicBufferClass * source)
{
	if (SeenBuff.Lock()) {
		WWPort_ModeX_Blit(WWPort_View_Of(source), WWPort_View_Of(&SeenBuff));
	}
	SeenBuff.Unlock();
}
