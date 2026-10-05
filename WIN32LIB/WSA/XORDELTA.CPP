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
**	XORDELTA.CPP -- PORT-CREATED. C translation of XORDELTA.ASM: the XOR-delta
**	("format 40") decoder behind WSA animations. Verified against the original
**	assembly: port/tests/asm_misc.cpp.
**
**	A delta is a stream of commands:
**	  n (1..127)          the next n bytes are XORed in ("dump")
**	  0, n, v             n bytes XORed with v ("run"; n is 1..255)
**	  0x80 + n (n>0)      skip n bytes
**	  0x80, w (16-bit LE):
**	      0               end of delta
**	      1..0x7FFF       skip w bytes
**	      0x8000 + n      dump n bytes          (n < 0x4000)
**	      0xC000 + n      run of n, value next  (n < 0x4000)
**	A count of 0 where a run or dump needs one made the assembly's counter wrap
**	(2^32 iterations); valid deltas never contain one, and here it does nothing.
*/

#include "wwstd.h"
#include "wsa.h"

namespace {

/*
**	Where the bytes go: straight on (Apply_XOR_Delta), or wrapping every
**	`width` bytes to the next row `nextrow` bytes on (the page/viewport form).
*/
struct Linear {
	unsigned char * p;
	void advance(uint32_t n) {p += n;}
	unsigned char & here() {return *p;}
	void step() {p++;}
};

struct Rows {
	unsigned char * p;
	uint32_t col, width, nextrow;
	void step()
	{
		p++;
		if (++col == width) {p = p - col + nextrow; col = 0;}
	}
	void advance(uint32_t n)
	{
		p -= col;
		col += n;
		while (col >= width) {col -= width; p += nextrow;}
		p += col;
	}
	unsigned char & here() {return *p;}
};

template <class Out, bool XOR>
void decode(Out & out, unsigned char const * s)
{
	for (;;) {
		uint32_t cmd = *s++;
		uint32_t count;
		bool run;
		if (cmd == 0) {
			count = *s++;
			run = true;
		} else if (cmd < 0x80) {
			count = cmd;
			run = false;
		} else if (cmd > 0x80) {
			out.advance(cmd - 0x80);
			continue;
		} else {
			uint32_t w = s[0] | (s[1] << 8);
			s += 2;
			if (w == 0) return;
			if (w < 0x8000) {out.advance(w); continue;}
			w -= 0x8000;
			run = (w & 0x4000) != 0;
			count = run ? w - 0x4000 : w;
		}
		if (count == 0) continue;
		if (run) {
			unsigned char v = *s++;
			while (count--) {
				if (XOR) out.here() ^= v; else out.here() = v;
				out.step();
			}
		} else {
			while (count--) {
				if (XOR) out.here() ^= *s; else out.here() = *s;
				s++;
				out.step();
			}
		}
	}
}

}

/*
**	Returns 0: the assembly returned EAX, which is 0 at the end command.
*/
extern "C" unsigned int __cdecl Apply_XOR_Delta(char * source_ptr, char * delta_ptr)
{
	Linear out = {(unsigned char *)source_ptr};
	decode<Linear, true>(out, (unsigned char const *)delta_ptr);
	return 0;
}

/*
**	copy == 0 (DO_XOR) XORs the delta in; anything else stores it (DO_COPY).
*/
extern "C" void __cdecl Apply_XOR_Delta_To_Page_Or_Viewport(void * target, void * delta, int width, int nextrow, int copy)
{
	Rows out = {(unsigned char *)target, 0, (uint32_t)width, (uint32_t)nextrow};
	if (copy == 0) {
		decode<Rows, true>(out, (unsigned char const *)delta);
	} else {
		decode<Rows, false>(out, (unsigned char const *)delta);
	}
}
