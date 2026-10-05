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
**	REVERSE.CPP -- PORT-CREATED. C translation of REVERSE.ASM: byte-order
**	reversal for the big-endian IFF/LBM formats. Verified against the original
**	assembly: port/tests/asm_misc.cpp.
**
**	The prototypes say `long`, which was 32 bits under Watcom; only the low 32
**	bits are reversed, and the result is that 32-bit value sign-extended, as a
**	Watcom long would have been.
*/

#include "wwstd.h"
#include "misc.h"

extern "C" long __cdecl Reverse_Long(long number)
{
	return (int32_t)__builtin_bswap32((uint32_t)number);
}

extern "C" short __cdecl Reverse_Short(short number)
{
	return (short)__builtin_bswap16((uint16_t)number);
}

extern "C" long __cdecl Swap_Long(long number)
{
	uint32_t v = (uint32_t)number;
	return (int32_t)((v >> 16) | (v << 16));
}
