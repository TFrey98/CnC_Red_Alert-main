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
**	SETSHAPE.CPP -- PORT-CREATED. C translation of SETSHAPE.ASM
**	(Set_Shape_Buffer), which also defines the two variables it sets.
**
**	_ShapeBuffer and _ShapeBufferSize lived in DRAWSHP.ASM's data segment. The
**	game uses them as a general scratch buffer (briefing text, shape frames);
**	Draw_Shape itself, the rest of DRAWSHP.ASM, is not called by the linked
**	game (port/LINK-CENSUS.md), so they are defined here.
*/

#include "wwstd.h"
#include "shape.h"

extern "C" {
long	_ShapeBufferSize = 0;
char *	_ShapeBuffer = NULL;
}

extern "C" void __cdecl Set_Shape_Buffer(void const * buffer, int size)
{
	_ShapeBufferSize = size;
	_ShapeBuffer = (char *)buffer;
}
