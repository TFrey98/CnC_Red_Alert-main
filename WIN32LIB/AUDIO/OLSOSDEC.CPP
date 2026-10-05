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
**	OLSOSDEC.CPP -- PORT-CREATED. C translation of OLSOSDEC.ASM
**	(General_sosCODECDecompressData): SOS ADPCM for the formats the 16-bit-mono
**	decoder (SOSCODEC.CPP) does not handle -- 8-bit output and stereo.
**	Verified against the original assembly: port/tests/asm_misc.cpp.
**
**	OLSOSDEC.ASM also defined sosCODECInitStream, as SOSCODEC.ASM did; the port
**	defines it once, in SOSCODEC.CPP.
*/

#include "wwstd.h"
#include "soscomp.h"
#include "ww_sos_adpcm.h"

extern "C" unsigned long __cdecl General_sosCODECDecompressData(_SOS_COMPRESS_INFO * info, unsigned long wBytes)
{
	return WWPortSOS::decompress(info, wBytes);
}
