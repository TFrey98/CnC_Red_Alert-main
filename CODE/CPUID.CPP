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
**	CPUID.CPP -- PORT-CREATED. Rebuild of CPUID.ASM (Detect_MMX_Availability,
**	CPUType, VendorID).
**
**	The assembly probed the x86 flags register and the CPUID instruction for
**	the processor family (3 = 386 ... 6 = Pentium Pro) and the MMX feature
**	bit. There is no x86 family on arm64, and no MMX: no code path in the port
**	uses MMX either. The values left are the assembly's own "could not
**	identify" defaults from its data segment. Only the multiplayer statistics
**	packet reads them (STATS.CPP; GETCPU.CPP's Get_CPU_Type has no callers).
*/

extern "C" {
char CPUType = 0;
char VendorID[19] = "Not available";

bool __cdecl Detect_MMX_Availability(void)
{
	return false;
}
}
