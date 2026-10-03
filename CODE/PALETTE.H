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
**	PALETTE.H -- the game's PaletteClass.
**
**	RECONSTRUCTED FOR THE ARM64 PORT. This file and PALETTE.CPP were not part of
**	EA's source release; the only trace of PaletteClass in the tree was a forward
**	declaration in RGB.H. The interface below was derived from the class's call
**	sites (about 200 of them across 18 files), not recovered from the original,
**	so treat its behaviour as a faithful reading of how the engine uses it rather
**	than as Westwood's code. See port/INVENTORY.md.
**
**	Why its absence went unnoticed: FUNCTION.H does `#include "palette.h"`, and
**	with no CODE/PALETTE.H that quoted include fell through -iquote to -I and
**	silently resolved to WIN32LIB/INCLUDE/PALETTE.H -- the library's C palette
**	functions, a different header that shares the name. That library header is
**	still wanted (Set_Palette and friends), so it is included explicitly below.
*/

#ifndef RA_PALETTECLASS_H
#define RA_PALETTECLASS_H

/*
**	The library header, by angle brackets so it resolves through -I and never
**	back to this file. Its include guard is PALETTE_H, which is why this file
**	deliberately uses a different one.
*/
#include	<palette.h>
#include	"rgb.h"

/*
**	LAYOUT IS LOAD-BEARING. The engine reads palettes straight off disk with
**	`(PaletteClass *)MFCD::Retrieve(name)`, copies them with memcpy/memmove using
**	sizeof(PaletteClass), and hands `&palette` to Set_Palette. So this class must
**	be exactly 256 RGBClass entries -- 768 bytes of 6-bit VGA triples, the .PAL
**	file format -- with no vtable and no other members. The static_assert in
**	PALETTE.CPP enforces that.
*/
class PaletteClass
{
	public:
		enum {
			COLOR_COUNT=256
		};

		PaletteClass(void) {};
		PaletteClass(RGBClass const & rgb);

		RGBClass & operator [] (int index) {return(Palette[index % COLOR_COUNT]);};
		RGBClass const & operator [] (int index) const {return(Palette[index % COLOR_COUNT]);};

		int operator == (PaletteClass const & palette) const;
		int operator != (PaletteClass const & palette) const {return(!(operator == (palette)));};

		/*
		**	The engine passes palettes to memcpy and to the C palette routines as
		**	raw bytes (e.g. `memcpy(save, CCPalette, ...)` in EGOS.CPP).
		*/
		operator unsigned char const * (void) const {return((unsigned char const *)&Palette[0]);};
		operator unsigned char * (void) {return((unsigned char *)&Palette[0]);};

		/*
		**	Move every entry `ratio`/256 of the way toward black, or toward the
		**	matching entry of `palette`.
		*/
		void Adjust(int ratio);
		void Adjust(int ratio, PaletteClass const & palette);

		/*
		**	As Adjust, but only for entries whose `lut` byte is non-zero. EGOS.CPP
		**	uses this to fade the credits background while the text stays put.
		*/
		void Partial_Adjust(int ratio, char * lut);
		void Partial_Adjust(int ratio, PaletteClass const & palette, char * lut);

		/*
		**	Fade the hardware palette from CurrentPalette to this one over `time`
		**	game ticks (0 means immediately), calling `callback` while it runs.
		*/
		void Set(int time = 0, void (* callback)(void) = 0) const;

		int Closest_Color(RGBClass const & rgb) const;

		/*
		**	The palette last sent to the hardware. Const because nothing should
		**	assign it directly except Set() -- RGBClass::Set in RGB.CPP writes
		**	single entries through a cast, which is the one sanctioned exception.
		*/
		static PaletteClass const CurrentPalette;

	protected:
		RGBClass Palette[COLOR_COUNT];
};

#endif
