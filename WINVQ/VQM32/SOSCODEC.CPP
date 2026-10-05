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
**	SOSCODEC.CPP -- PORT-CREATED. C translation of the movie player's
**	SOSCODEC.ASM: VQA_sosCODECInitStream and VQA_sosCODECDecompressData, the
**	ADPCM decoder for VQA soundtracks. The same algorithm as the library's
**	(port/compat/ww_sos_adpcm.h) over the movie player's own struct layout.
**	Verified against the original assembly: port/tests/asm_winvq.cpp.
**
**	(VQA_sosCODECCompressData is not carried over: the player never
**	compresses.)
*/

#include <vqm32/soscomp.h>
#include "ww_sos_adpcm.h"

extern "C" void __cdecl VQA_sosCODECInitStream(_SOS_COMPRESS_INFO * info)
{
	WWPortSOS::init(info);
}

extern "C" unsigned long __cdecl VQA_sosCODECDecompressData(_SOS_COMPRESS_INFO * info, unsigned long wBytes)
{
	return WWPortSOS::decompress(info, wBytes);
}
