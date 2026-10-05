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
**	SOSCODEC.CPP -- PORT-CREATED. C translation of SOSCODEC.ASM:
**	sosCODECInitStream and sosCODECDecompressData, the sound-effect ADPCM
**	decoder (SOUNDINT.CPP uses it for 16-bit mono, General_ for the rest).
**	Verified against the original assembly: port/tests/asm_misc.cpp.
**
**	The assembly is a table-driven, unrolled decoder; this computes the same
**	samples with the plain step-table algorithm (ww_sos_adpcm.h), which the
**	tests show is output-identical across chunked calls.
**
**	It replaces CODE/ADPCM.CPP in the build: that file is Westwood's own C
**	version of these two functions, but not what the shipped library linked,
**	and its sosCODECInitStream left the step index and step alone -- state
**	General_sosCODECDecompressData reads for 8-bit and stereo sounds.
*/

#include "wwstd.h"
#include "soscomp.h"
#include "soundint.h"
#include "ww_sos_adpcm.h"

/*
**	Both SOSCODEC.ASM and OLSOSDEC.ASM defined this. They agree on the step
**	indices and predictors; OLSOSDEC's also sets the steps to 7 and the sample
**	indices to 0. That is the IMA starting state, and the only one under which
**	General_'s first sample does not depend on the previous sound, so it is
**	the one used here.
*/
extern "C" void __cdecl sosCODECInitStream(_SOS_COMPRESS_INFO * info)
{
	WWPortSOS::init(info);
}

/*
**	Only 16-bit mono: for any other format the assembly returned without
**	decoding (SOUNDINT.CPP never asks it to).
*/
extern "C" unsigned long __cdecl sosCODECDecompressData(_SOS_COMPRESS_INFO * info, unsigned long wBytes)
{
	if (info->wBitSize != 16 || info->wChannels != 1) return 0;
	return WWPortSOS::decompress(info, wBytes);
}

/*
**	DPMI page locking for the decoder's code and tables, so a DOS interrupt
**	handler could run it. Nothing to lock on macOS; success, as the assembly
**	returned when DPMI agreed.
*/
extern "C" int __cdecl sosCODEC_Lock(void)
{
	return 1;
}

extern "C" int __cdecl sosCODEC_Unlock(void)
{
	return 1;
}
