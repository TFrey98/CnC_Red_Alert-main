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
**	PALETTE.CPP -- RECONSTRUCTED for the arm64 port; see the note in PALETTE.H.
**
**	Everything here is built on RGBClass::Adjust and RGBClass::Difference, which
**	DID survive in RGB.CPP, so the colour arithmetic itself is Westwood's. What
**	is reconstructed is the iteration over 256 entries and the fade loop in Set.
*/

#include	"function.h"

static_assert(sizeof(RGBClass) == 3, "RGBClass must be three packed 6-bit guns");
static_assert(sizeof(PaletteClass) == PaletteClass::COLOR_COUNT * 3,
	"PaletteClass must be exactly the 768-byte .PAL layout: it is read off disk by cast");

PaletteClass const PaletteClass::CurrentPalette;


PaletteClass::PaletteClass(RGBClass const & rgb)
{
	for (int index = 0; index < COLOR_COUNT; index++) {
		Palette[index] = rgb;
	}
}


int PaletteClass::operator == (PaletteClass const & palette) const
{
	if (this == &palette) return(true);
	return(memcmp(&Palette[0], &palette.Palette[0], sizeof(Palette)) == 0);
}


void PaletteClass::Adjust(int ratio)
{
	RGBClass const black;
	for (int index = 0; index < COLOR_COUNT; index++) {
		Palette[index].Adjust(ratio, black);
	}
}


void PaletteClass::Adjust(int ratio, PaletteClass const & palette)
{
	for (int index = 0; index < COLOR_COUNT; index++) {
		Palette[index].Adjust(ratio, palette[index]);
	}
}


void PaletteClass::Partial_Adjust(int ratio, char * lut)
{
	RGBClass const black;
	for (int index = 0; index < COLOR_COUNT; index++) {
		if (lut[index]) {
			Palette[index].Adjust(ratio, black);
		}
	}
}


void PaletteClass::Partial_Adjust(int ratio, PaletteClass const & palette, char * lut)
{
	for (int index = 0; index < COLOR_COUNT; index++) {
		if (lut[index]) {
			Palette[index].Adjust(ratio, palette[index]);
		}
	}
}


int PaletteClass::Closest_Color(RGBClass const & rgb) const
{
	int closest = 0;
	int value = -1;

	for (int index = 0; index < COLOR_COUNT; index++) {
		int difference = rgb.Difference(Palette[index]);
		if (value == -1 || difference < value) {
			value = difference;
			closest = index;
		}
	}
	return(closest);
}


/*
**	Fades from CurrentPalette to this palette over `time` ticks.
**
**	Each pass builds an intermediate palette proportional to elapsed time and
**	sends it to the hardware, calling `callback` so the engine can keep its
**	world ticking during the fade -- 11 call sites pass Call_Back for exactly
**	that reason. A pass is only made once per timer tick, since a second pass in
**	the same tick would compute the identical intermediate palette.
**
**	The final Set_Palette is unconditional so the fade always lands exactly on
**	the target, whatever rounding the intermediate steps accumulated.
*/
void PaletteClass::Set(int time, void (* callback)(void)) const
{
	if (time > 0) {
		CDTimerClass<SystemTimerClass> timer = time;
		PaletteClass const original = CurrentPalette;

		while (timer) {
			PaletteClass palette = original;
			palette.Adjust(((time - (int)timer) * 256) / time, *this);
			Set_Palette((void *)&palette[0]);

			if (callback != 0) callback();

			unsigned long holdtime = timer;
			while (timer && timer == holdtime) {
				if (callback != 0) callback();
			}
		}
	}

	Set_Palette((void *)&(*this)[0]);
	(PaletteClass &)CurrentPalette = *this;
}
