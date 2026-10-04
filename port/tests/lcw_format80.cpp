/*
** lcw_format80.cpp -- both of the engine's C LCW ("Format80") decoders against
** the format as described independently at https://multimedia.cx/vqa_overview.htm:
**
**   1  10cccccc                     copy the next c bytes verbatim (c == 0: end)
**   2  0cccpppp pppppppp            copy c+3 bytes from (dest - p)
**   3  11cccccc pppppppp pppppppp   copy c+3 bytes from absolute position p
**   4  11111110 cccc(16) vv         fill c bytes with v
**   5  11111111 cccc(16) pppp(16)   copy c bytes from absolute position p
**
** Random command streams are generated from that description, with the expected
** output computed alongside, and each engine decoder must reproduce it exactly.
** Copies may overlap their own output -- which is how LCW encodes runs.
*/
#include "lcw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
extern "C" unsigned long LCW_Uncompress(void * source, void * dest, unsigned long length);	/* C linkage, like the assembly it replaces */

static unsigned rnd(unsigned n) { return (unsigned)(rand() % n); }

/* Builds one stream; returns the expected decoded bytes. */
static std::vector<unsigned char> make(std::vector<unsigned char> & enc, int commands) {
	std::vector<unsigned char> out;
	enc.clear();
	for (int i = 0; i < commands; i++) {
		unsigned kind = out.empty() ? 1 : 1 + rnd(5);
		if (kind == 1) {						/* verbatim */
			unsigned c = 1 + rnd(63);
			enc.push_back(0x80 | c);
			for (unsigned k = 0; k < c; k++) { unsigned char b = rnd(256); enc.push_back(b); out.push_back(b); }
		} else if (kind == 2) {					/* relative copy, may overlap */
			unsigned c = 3 + rnd(8);
			unsigned p = 1 + rnd(out.size() < 4095 ? out.size() : 4095);
			enc.push_back((unsigned char)(((c - 3) << 4) | (p >> 8))); enc.push_back(p & 0xFF);
			size_t from = out.size() - p;
			for (unsigned k = 0; k < c; k++) out.push_back(out[from + k]);
		} else if (kind == 3) {					/* absolute copy, short */
			unsigned c = 3 + rnd(59);			/* 0xC0..0xFD: c-3 <= 58 so it never collides with 0xFE/0xFF */
			unsigned p = rnd(out.size() < 65535 ? out.size() : 65535);
			enc.push_back(0xC0 | (c - 3)); enc.push_back(p & 0xFF); enc.push_back(p >> 8);
			for (unsigned k = 0; k < c; k++) out.push_back(out[p + k]);
		} else if (kind == 4) {					/* fill */
			unsigned c = 1 + rnd(300); unsigned char v = rnd(256);
			enc.push_back(0xFE); enc.push_back(c & 0xFF); enc.push_back(c >> 8); enc.push_back(v);
			for (unsigned k = 0; k < c; k++) out.push_back(v);
		} else {								/* absolute copy, long */
			unsigned c = 1 + rnd(300);
			unsigned p = rnd(out.size() < 65535 ? out.size() : 65535);
			enc.push_back(0xFF); enc.push_back(c & 0xFF); enc.push_back(c >> 8); enc.push_back(p & 0xFF); enc.push_back(p >> 8);
			for (unsigned k = 0; k < c; k++) out.push_back(out[p + k]);
		}
	}
	enc.push_back(0x80);						/* end of data */
	return out;
}

int main() {
	srand(80);
	int ok_a = 0, ok_b = 0, len_a = 0, total = 0;
	for (int trial = 0; trial < 3000; trial++) {
		std::vector<unsigned char> enc;
		std::vector<unsigned char> want = make(enc, 1 + rnd(40));
		if (want.size() > 60000) continue;		/* absolute positions are 16-bit */
		total++;
		std::vector<unsigned char> a(want.size() + 64, 0xCD), b(want.size() + 64, 0xCD);
		unsigned long na = LCW_Uncompress(enc.data(), a.data(), (unsigned long)want.size());
		LCW_Uncomp(enc.data(), b.data(), (unsigned long)want.size());
		if (memcmp(a.data(), want.data(), want.size()) == 0) ok_a++;
		if (na == want.size()) len_a++;
		if (memcmp(b.data(), want.data(), want.size()) == 0) ok_b++;
	}
	printf("  LCW_Uncompress (LCWUNCMP.CPP: shapes, icons, palettes) %d/%d streams exact\n", ok_a, total);
	printf("  LCW_Uncompress returns the decoded length           %d/%d\n", len_a, total);
	printf("  LCW_Uncomp     (LCW.CPP: streamed and saved data)      %d/%d streams exact\n", ok_b, total);
	bool pass = ok_a == total && ok_b == total && len_a == total;
	printf("%s\n", pass ? "lcw_format80: all pass" : "FAILED");
	return !pass;
}
