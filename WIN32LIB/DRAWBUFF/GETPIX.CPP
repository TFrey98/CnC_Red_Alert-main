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
**	GETPIX.CPP -- PORT-CREATED. C translation of GETPIX.ASM (Buffer_Get_Pixel).
**	Verified against the original assembly: port/tests/asm_drawbuff.cpp.
**	FTPUTPIX.ASM beside it holds an identical Buffer_Get_Pixel, but the
**	library's MAKEFILE builds this one.
*/

#include "gbuffer.h"
#include "ww_view.h"

/*
**	Off-view coordinates (an unsigned compare, so negatives too) do NOT read as
**	0: the assembly leaves the coordinate it was testing in EAX and returns
**	that -- x if x is out of range, else y. Found by running the original under
**	the emulator; kept because callers index tables with the result.
*/
int WWPort_Buffer_Get_Pixel(WWPortView const & v, int x, int y)
{
	if ((uint32_t)x >= (uint32_t)v.Width) return x;
	if ((uint32_t)y >= (uint32_t)v.Height) return y;
	return v.Offset[x + (size_t)y * (uint32_t)v.Stride()];
}

extern "C" int __cdecl Buffer_Get_Pixel(void * thisptr, int x, int y)
{
	return WWPort_Buffer_Get_Pixel(WWPort_View_Of((GraphicViewPortClass *)thisptr), x, y);
}
