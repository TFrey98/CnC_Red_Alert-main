/*
**	asm_replay.h -- replay vectors recorded from Westwood's ORIGINAL assembly
**	(port/asmref/gen_vectors.py, run under an x86 emulator) against the C
**	translations. fill() and fnv() must stay identical to gen_vectors.py.
*/
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "ww_view.h"

#ifndef ASM_VECTORS
#error "build with -DASM_VECTORS=\"<dir>\""
#endif

static void fill(unsigned char * p, size_t n, uint32_t seed)
{
	uint32_t s = seed ? seed : 0x9E3779B9u;
	for (size_t i = 0; i < n; i++) {
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		p[i] = (unsigned char)(s >> 24);
	}
}

static uint64_t fnv(unsigned char const * p, size_t n, uint64_t h = 0xCBF29CE484222325ull)
{
	for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
	return h;
}

struct Case {
	int line;
	std::vector<long long> in;
	std::vector<unsigned long long> out;
};

static std::vector<Case> load(char const * routine)
{
	std::string path = std::string(ASM_VECTORS) + "/" + routine + ".txt";
	FILE * f = fopen(path.c_str(), "r");
	if (!f) {printf("missing %s\n", path.c_str()); exit(1);}
	std::vector<Case> cases;
	char buf[8192];
	int line = 0;
	while (fgets(buf, sizeof(buf), f)) {
		line++;
		Case c; c.line = line;
		char * colon = strchr(buf, ':');
		if (!colon) continue;
		*colon = 0;
		for (char * t = strtok(buf, " \n"); t; t = strtok(NULL, " \n")) c.in.push_back(strtoll(t, NULL, 10));
		for (char * t = strtok(colon + 1, " \n"); t; t = strtok(NULL, " \n")) c.out.push_back(strtoull(t, NULL, 10));
		cases.push_back(c);
	}
	fclose(f);
	return cases;
}

enum {GUARD = 64};

/* A view port laid out exactly as gen_vectors.py's View: guard, lines, guard. */
struct TestView {
	std::vector<unsigned char> mem;
	WWPortView v;
	TestView(long long const * p)		// w h xadd pitch align seed
	{
		int w = (int)p[0], h = (int)p[1], xadd = (int)p[2], pitch = (int)p[3], align = (int)p[4];
		mem.resize(GUARD + align + (size_t)(w + xadd + pitch) * h + GUARD);
		fill(mem.data(), mem.size(), (uint32_t)p[5]);
		v.Offset = mem.data() + GUARD + align;
		v.Width = w; v.Height = h; v.XAdd = xadd; v.Pitch = pitch; v.XPos = 0; v.YPos = 0;
	}
	uint64_t hash() const {return fnv(mem.data(), mem.size());}
	enum {PARAMS = 6};
};

/* gen_vectors.py's linear(): w x h pixels between guards, filled from seed. */
struct TestLinear {
	std::vector<unsigned char> mem;
	TestLinear(long long w, long long h, uint32_t seed)
	{
		size_t n = (w > -4096 && w < 4096 && h > -4096 && h < 4096) ? (size_t)(w > 0 ? w : 0) * (size_t)(h > 0 ? h : 0) : 0;
		mem.resize(GUARD + n + GUARD);
		fill(mem.data(), mem.size(), seed);
	}
	unsigned char * addr() {return mem.data() + GUARD;}
	uint64_t hash() const {return fnv(mem.data(), mem.size());}
};

/* gen_vectors.py's Block: stride x rows between guards; views are windows. */
struct TestBlock {
	std::vector<unsigned char> mem;
	int stride;
	TestBlock(long long stride_, long long rows, uint32_t seed) : stride((int)stride_)
	{
		mem.resize(GUARD + (size_t)stride_ * rows + GUARD);
		fill(mem.data(), mem.size(), seed);
	}
	WWPortView view(long long ox, long long oy, long long w, long long h)
	{
		WWPortView v;
		v.Offset = mem.data() + GUARD + oy * stride + ox;
		v.Width = (int)w; v.Height = (int)h; v.XAdd = stride - (int)w; v.Pitch = 0; v.XPos = 0; v.YPos = 0;
		return v;
	}
	uint64_t hash() const {return fnv(mem.data(), mem.size());}
};

/* gen_vectors.py's iconset(): IControl_Type, icons, 256-byte map, transparency flags. */
static std::vector<unsigned char> make_iconset(int w, int h, int count, uint32_t seed)
{
	size_t nicons = (size_t)count * w * h;
	std::vector<unsigned char> body(nicons + 256 + count);
	fill(body.data(), body.size(), seed);
	for (int i = 0; i < 256; i++) body[nicons + i] %= (count + 2);
	for (int i = 0; i < count; i++) body[nicons + 256 + i] &= 1;
	int32_t const icons = 40, mapo = 40 + (int32_t)nicons, trans = mapo + 256, size = 40 + (int32_t)body.size();
	int16_t const h16[6] = {(int16_t)w, (int16_t)h, (int16_t)count, 0, 0, 0};
	int32_t const h32[7] = {size, icons, 0, 0, trans, 0, mapo};
	std::vector<unsigned char> out(40);
	memcpy(out.data(), h16, 12);
	memcpy(out.data() + 12, h32, 28);
	out.insert(out.end(), body.begin(), body.end());
	return out;
}

/* gen_vectors.py's _gen_snd1 cases: count dseed samples len data... */
template <class F>
static bool snd1_case(Case const & c, F decode)
{
	long long const * p = &c.in[0];
	std::vector<unsigned char> src(p[3] + 64, 0), dst(p[2] + 2 * GUARD);
	for (long long i = 0; i < p[3]; i++) src[i] = (unsigned char)p[4 + i];
	fill(dst.data(), dst.size(), (uint32_t)p[1]);
	long r = decode(src.data(), dst.data() + GUARD, (long)p[0]);
	return (int32_t)r == (int32_t)(long long)c.out[0] && fnv(dst.data(), dst.size()) == c.out[1];
}

/*
**	gen_vectors.py's _gen_sos cases: bits chans use_init start[6] seed dseed
**	nchunks chunks... -> output hash, final state[6], return per call.
**	full == false compares the output only (the table-driven decoder keeps
**	its state in another form).
*/
template <class Info, class Init, class Decomp>
static bool sos_case(Case const & c, Init init, Decomp decomp, bool full)
{
	long long const * p = &c.in[0];
	int bits = (int)p[0], chans = (int)p[1];
	long long nchunks = p[11];
	long long total = 0;
	for (long long i = 0; i < nchunks; i++) total += p[12 + i];
	size_t src_len = (size_t)(total / (bits == 16 ? 2 : 1) / 2 + 4);
	std::vector<unsigned char> src(src_len + 64), dst(total + 2 * GUARD);
	fill(src.data(), src.size(), (uint32_t)p[9]);
	fill(dst.data(), dst.size(), (uint32_t)p[10]);
	Info info;
	memset(&info, 0, sizeof(info));
	info.wBitSize = (short)bits;
	info.wChannels = (short)chans;
	if (p[2]) {
		init(&info);
	} else {
		info.dwPredicted = (long)p[3]; info.wIndex = (short)p[4]; info.wStep = (short)p[5];
		info.dwPredicted2 = (long)p[6]; info.wIndex2 = (short)p[7]; info.wStep2 = (short)p[8];
	}
	unsigned char * sp = src.data();
	unsigned char * dp = dst.data() + GUARD;
	bool same = true;
	for (long long i = 0; i < nchunks; i++) {
		info.lpSource = (char *)sp;
		info.lpDest = (char *)dp;
		unsigned long r = decomp(&info, (unsigned long)p[12 + i]);
		if (full) same = same && (uint32_t)r == (uint32_t)c.out[7 + i];
		sp += p[12 + i] / (bits == 16 ? 2 : 1) / 2;
		dp += p[12 + i];
	}
	same = same && fnv(dst.data(), dst.size()) == c.out[0];
	if (full) {
		long long const state[6] = {(int32_t)info.dwPredicted, info.wIndex, info.wStep, (int32_t)info.dwPredicted2, info.wIndex2, info.wStep2};
		for (int i = 0; i < 6; i++) same = same && state[i] == (long long)c.out[1 + i];
	}
	return same;
}

struct Tally {
	char const * name; int pass = 0, fail = 0;
	explicit Tally(char const * n) : name(n) {}
	void check(Case const & c, bool ok, char const * what = "")
	{
		if (ok) {pass++; return;}
		if (fail++ < 5) {
			printf("  %s line %d differs from the original assembly %s:", name, c.line, what);
			for (long long x : c.in) printf(" %lld", x);
			printf("\n");
		}
	}
	bool report() const
	{
		printf("  %-28s %4d/%d match the original\n", name, pass, pass + fail);
		return fail == 0 && pass > 0;
	}
};
