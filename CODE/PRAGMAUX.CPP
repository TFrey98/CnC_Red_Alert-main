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
**	PRAGMAUX.CPP -- PORT-CREATED. C bodies for functions the engine defined with
**	Watcom's `#pragma aux`: inline x86 assembly written inside a header. Clang
**	ignores the pragma and keeps only the declaration, so these were undefined at
**	link time; none of them lives in a .ASM file, which is why the assembly
**	inventory never counted them.
**
**	Each is a literal translation of the assembly quoted beside it, including
**	quirks, because callers depend on exact results (unit movement goes through
**	calcx/calcy and the Fixed/Cardinal conversions). See port/tests/pragma_aux.cpp.
*/

#include	"function.h"
#include	<mach/mach_time.h>

/*
**	Bit arrays (JSHELL.H). The assembly addresses bit n as bit (n & 31) of the
**	little-endian dword at index (n >> 5) -- the same bit as byte (n >> 3), bit
**	(n & 7), which is how it is done here.
*/
void Set_Bit(void * array, int bit, int value)
{
	unsigned char * p = (unsigned char *)array + ((unsigned)bit >> 3);
	unsigned char mask = (unsigned char)(1u << (bit & 7));
	if (value) *p |= mask; else *p &= (unsigned char)~mask;		// btr, then bts if value
}

int Get_Bit(void const * array, int bit)
{
	return (((unsigned char const *)array)[(unsigned)bit >> 3] >> (bit & 7)) & 1;	// bt + setc
}

/* Unbounded scans, exactly like the bsf loops: the caller guarantees a hit. */
int First_True_Bit(void const * array)
{
	unsigned char const * p = (unsigned char const *)array;
	for (int base = 0; ; base += 32, p += 4) {
		uint32_t dword;
		memcpy(&dword, p, sizeof(dword));
		if (dword) return base + __builtin_ctz(dword);
	}
}

int First_False_Bit(void const * array)
{
	unsigned char const * p = (unsigned char const *)array;
	for (int base = 0; ; base += 32, p += 4) {
		uint32_t dword;
		memcpy(&dword, p, sizeof(dword));
		if (~dword) return base + __builtin_ctz(~dword);
	}
}

/*
**	Fixed-point conversions (JSHELL.H).
**
**	Fixed_To_Cardinal:  mul edx / add eax,080h / test eax,0FF000000h /
**	                    jz ok / mov eax,000FFFFFFh / ok: shr eax,8
**	Only the LOW 32 bits of the product take part (EDX is discarded), the add
**	wraps at 32 bits, and the saturation test happens before the shift.
*/
unsigned Fixed_To_Cardinal(unsigned base, unsigned fixed)
{
	uint32_t value = (uint32_t)(base * fixed) + 0x80u;
	if (value & 0xFF000000u) value = 0x00FFFFFFu;
	return value >> 8;
}

/*
**	Cardinal_To_Fixed:  or ebx,ebx / jz fini / shl eax,8 / xor edx,edx / div ebx
**	A zero base returns the cardinal unchanged (EAX untouched).
*/
unsigned Cardinal_To_Fixed(unsigned base, unsigned cardinal)
{
	if (base == 0) return cardinal;
	return (uint32_t)(cardinal << 8) / base;
}

/*
**	Movement deltas (COORD.CPP): imul bx / shl ax,1 / rcl dx,1 / mov al,ah /
**	mov ah,dl -- i.e. AX = bits 7..22 of the 32-bit product. The result is that
**	16-bit value sign-extended; the OBSOLETE Borland path beside the call sites
**	(`_DX = _AX; x += _DX;`) shows that is how callers use it. calcy negates it
**	("subtraction needed because of inverted sine table").
*/
int calcx(signed short value, short distance)
{
	return (int)(short)(((int)value * (int)distance) >> 7);
}

int calcy(signed short value, short distance)
{
	return (int)(short)-(int)(short)(((int)value * (int)distance) >> 7);
}

/*
**	CPU clock (MPU.H): `rdtsc`, returning the low half and storing the high
**	half. arm64 has no cycle counter visible to user code; the monotonic system
**	counter is the faithful equivalent for the only use -- measuring elapsed time.
*/
unsigned long Get_CPU_Clock(unsigned long & high)
{
	uint64_t now = mach_absolute_time();
	high = (unsigned long)(uint32_t)(now >> 32);
	return (unsigned long)(uint32_t)now;
}

/*
**	VGA palette hardware (RGB.H). RGBClass::Set programs one palette entry the
**	DOS way: write the index to port 3C8h, then red/green/blue to 3C9h. That
**	"hardware" is emulated here as a 256-entry DAC (6-bit components, as on VGA)
**	with the write index auto-incrementing after each blue, exactly as the chip
**	did. Each completed colour is also handed to the DirectDraw emulation
**	(port/compat/win32_ddraw.cpp), so the screen's palette follows the DAC.
*/
unsigned char WWPort_VGA_DAC[256][3];
extern "C" void WWPort_DAC_Entry(int index, int red, int green, int blue);	// port/compat/win32_ddraw.cpp
static unsigned char wwport_dac_index;
static unsigned char wwport_dac_component;

void outportb(int port, unsigned char data)
{
	switch (port) {
		case 0x3C8:								// DAC write index
			wwport_dac_index = data;
			wwport_dac_component = 0;
			break;
		case 0x3C9:								// DAC data: R, G, B, then next entry
			WWPort_VGA_DAC[wwport_dac_index][wwport_dac_component] = (unsigned char)(data & 0x3F);
			if (++wwport_dac_component == 3) {
				unsigned char const * c = WWPort_VGA_DAC[wwport_dac_index];
				WWPort_DAC_Entry(wwport_dac_index, c[0], c[1], c[2]);		// the screen's palette follows
				wwport_dac_component = 0;
				wwport_dac_index++;
			}
			break;
		default:								// other VGA registers have no meaning here
			break;
	}
}

void outport(int port, unsigned short data)
{
	outportb(port, (unsigned char)(data & 0xFF));
	outportb(port + 1, (unsigned char)(data >> 8));
}

void outrgb(unsigned char red, unsigned char green, unsigned char blue)
{
	outportb(0x3C9, red);
	outportb(0x3C9, green);
	outportb(0x3C9, blue);
}

/*
**	(`output`, the monochrome-monitor port write, was also given a body here
**	until WINSTUB.CPP joined the build: Westwood's own empty one is there.)
*/
