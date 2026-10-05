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
**	VB.CPP -- PORT-CREATED. Native replacement for VB.ASM (WaitVB, WaitNoVB)
**	and for TESTVB.CPP's TestVBIBit, which the library build drops.
**
**	All three read the VGA input-status register (port 3DAh) to time palette
**	changes and page flips to the vertical retrace. A Mac has no such register:
**	frames reach the screen through the Metal backend, which presents on the
**	display's refresh, and the player's own timer paces the movie. So there is
**	nothing to wait for, and the polarity test has nothing to measure.
*/

#include <vqm32/video.h>

extern "C" void __cdecl WaitNoVB(short vbibit)
{
	(void)vbibit;
}

extern "C" void __cdecl WaitVB(short vbibit)
{
	(void)vbibit;
}

/*
**	0: "bit set means in retrace", the usual VGA polarity. With the waits above
**	doing nothing, the value only has to be one the player accepts.
*/
long TestVBIBit(void)
{
	return 0;
}
