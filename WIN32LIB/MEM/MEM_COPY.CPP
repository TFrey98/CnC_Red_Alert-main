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
**	MEM_COPY.CPP -- PORT-CREATED. C translation of MEM_COPY.ASM (Mem_Copy).
**	Verified against the original assembly, including overlapping copies:
**	port/tests/asm_misc.cpp.
*/

#include "wwstd.h"
#include "wwmem.h"
#include <string.h>

/*
**	An overlap-safe copy (it picks the direction by comparing the addresses,
**	which is memmove's contract), that does nothing if the two addresses are
**	equal or either is NULL.
*/
extern "C" void __cdecl Mem_Copy(void const * source, void * dest, unsigned long bytes_to_copy)
{
	if (source == dest || source == NULL || dest == NULL) return;
	memmove(dest, source, (uint32_t)bytes_to_copy);
}
