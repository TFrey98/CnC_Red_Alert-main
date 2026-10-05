/*
**	asm_game.cpp -- C translations of the GAME's own assembly (CODE/*.ASM) vs
**	vectors recorded from the original assembly (port/asmref). Built with the
**	game's flags, as those translations are.
*/
#include "asm_replay.h"
#include <font.h>

extern "C" unsigned char ColorXlat[256];
unsigned char * WWPort_Buffer_Print(WWPortView const & v, char const * string, int x, int y, int fcolor, int bcolor);
void WWPort_ModeX_Blit(WWPortView const & src, WWPortView const & dst);
extern "C" void __cdecl Asm_Interpolate(unsigned char *, unsigned char *, int, int, int);
extern "C" void __cdecl Asm_Interpolate_Line_Double(unsigned char *, unsigned char *, int, int, int);
extern "C" void __cdecl Asm_Interpolate_Line_Interpolate(unsigned char *, unsigned char *, int, int, int);

extern "C" int __cdecl LCW_Comp(void const * source, void * dest, int datasize);
extern "C" unsigned long __cdecl LCW_Uncompress(void * source, void * dest, unsigned long length);

/* gen_vectors.py's lcw_data(). */
static std::vector<unsigned char> lcw_data(uint32_t seed, int n)
{
	std::vector<unsigned char> d(n);
	fill(d.data(), n, seed);
	unsigned char ctl[128];
	fill(ctl, 128, seed ^ 0x13579BDFu);
	for (int j = 0; j < 128; j += 4) {
		int a = ctl[j], b = ctl[j + 1], c = ctl[j + 2], kind = ctl[j + 3];
		int pos = a * n / 256;
		int length = n - pos < (b * c) % 400 + 1 ? n - pos : (b * c) % 400 + 1;
		if (kind % 3 == 0) {
			for (int i = 0; i < length; i++) d[pos + i] = (unsigned char)b;
		} else if (kind % 3 == 1) {
			int src = c * n / 256;
			for (int i = 0; i < length; i++) if (src + i < n) d[pos + i] = d[src + i];
		} else {
			for (int i = 0; i < length; i++) d[pos + i] &= 3;
		}
	}
	return d;
}

struct WWPortFrameArgs {
	void const * GhostTable; void const * FadingTable; int FadingCount; int Predator; int Partial;
};
long WWPort_Buffer_Frame_To_Page(int x, int y, int w, int h, void * buffer, WWPortView const & v, int flags,
	WWPortFrameArgs const & a);

/* 2KEYFRAM.CPP's and CONQUER.CPP's shape-buffer globals, stood in for here. */
extern "C" {
char * BigShapeBufferStart = NULL;
char * TheaterShapeBufferStart = NULL;
BOOL UseBigShapeBuffer = FALSE;
bool UseOldShapeDraw = false;
}

/* gen_vectors.py's frame_tables() and frame_pixels(). */
static void frame_tables(uint32_t seed, int nrows, std::vector<unsigned char> & ghost, std::vector<unsigned char> & fadet)
{
	ghost.assign(256 + 65536, 0);
	fill(ghost.data(), 256, seed);
	for (int i = 0; i < 256; i++) ghost[i] = (ghost[i] & 1) ? 0xFF : (unsigned char)(ghost[i] % nrows);
	fill(ghost.data() + 256, 65536, seed ^ 0x2468ACE0u);
	fadet.assign(256, 0);
	fill(fadet.data(), 256, seed ^ 0x0F0F0F0Fu);
}

static std::vector<unsigned char> frame_pixels(uint32_t seed, int w, int h)
{
	std::vector<unsigned char> px((size_t)w * h);
	fill(px.data(), px.size(), seed);
	for (size_t i = 0; i < px.size(); i++) if (px[i] % 3 == 0) px[i] = 0;
	for (int row = 0; row < h; row++) if (px[(size_t)row * w] == 7) memset(&px[(size_t)row * w], 0, w);
	return px;
}

/* INTERPAL.CPP's table, stood in for here. */
unsigned char PaletteInterpolationTable[256][256];

/* LOADFONT.CPP's globals, stood in for here. */
int FontXSpacing = 0;
int FontYSpacing = 0;
void const * FontPtr = NULL;

/* gen_vectors.py's make_font(). */
static std::vector<unsigned char> make_font(uint32_t seed, int maxh)
{
	unsigned char rnd[768];
	fill(rnd, sizeof(rnd), seed);
	int const info = 16, widths = 24, heights = 280, offsets = 792, data = 1304;
	int w[256], top[256], ch[256], offs[256], pos = data;
	for (int c = 0; c < 256; c++) {
		w[c] = 1 + rnd[c] % 12;
		top[c] = rnd[256 + c] % (maxh + 1);
		ch[c] = rnd[512 + c] % (maxh - top[c] + 1);
	}
	for (int c = 0; c < 256; c++) {
		offs[c] = pos;
		pos += ((w[c] + 1) / 2) * ch[c];
	}
	std::vector<unsigned char> f(pos);
	uint16_t const hdr[7] = {(uint16_t)pos, 0, info, offsets, widths, data, heights};
	memcpy(f.data(), hdr, sizeof(hdr));
	f[info + 4] = (unsigned char)maxh;
	f[info + 5] = 12;
	for (int c = 0; c < 256; c++) {
		f[widths + c] = (unsigned char)w[c];
		f[heights + 2 * c] = (unsigned char)top[c];
		f[heights + 2 * c + 1] = (unsigned char)ch[c];
		f[offsets + 2 * c] = (unsigned char)offs[c];
		f[offsets + 2 * c + 1] = (unsigned char)(offs[c] >> 8);
	}
	fill(f.data() + data, pos - data, seed ^ 0xA5A5A5A5u);
	return f;
}

int main()
{
	bool ok = true;
	{
		Tally t("Buffer_Print");
		for (Case const & c : load("buffer_print")) {
			long long const * p = &c.in[0];
			TestView tv(p);
			std::vector<unsigned char> font;
			if (p[6]) font = make_font((uint32_t)p[6], (int)p[7]);
			FontPtr = p[6] ? font.data() : NULL;
			FontXSpacing = (int)p[8]; FontYSpacing = (int)p[9];
			std::string text;
			for (long long i = 0; i < p[15]; i++) text += (char)p[16 + i];
			bool same = true;
			if (p[14]) same = fnv(ColorXlat, 241) == c.out[3];		// initial table == the assembly's data
			unsigned char * r = WWPort_Buffer_Print(tv.v, text.c_str(), (int)p[10], (int)p[11], (int)p[12], (int)p[13]);
			long long rel = r ? (long long)(r - tv.v.Offset) : -(1ll << 40);
			same = same && rel == (long long)c.out[0] && tv.hash() == c.out[1] && fnv(ColorXlat, 241) == c.out[2];
			t.check(c, same);
		}
		ok &= t.report();
	}
	{
		Tally t("Asm_Interpolate (3 copy types)");
		for (Case const & c : load("asm_interpolate")) {
			long long const * p = &c.in[0];		// kind tseed sseed dseed w h pitch
			int w = (int)p[4], h = (int)p[5], pitch = (int)p[6];
			fill(&PaletteInterpolationTable[0][0], 65536, (uint32_t)p[1]);
			std::vector<unsigned char> src((size_t)w * (h + 1));
			fill(src.data(), src.size(), (uint32_t)p[2]);
			std::vector<unsigned char> dst(GUARD + (size_t)pitch * 2 * (h + 1) + GUARD);
			fill(dst.data(), dst.size(), (uint32_t)p[3]);
			switch (p[0]) {
				case 0: Asm_Interpolate(src.data(), dst.data() + GUARD, h, w, pitch); break;
				case 1: Asm_Interpolate_Line_Double(src.data(), dst.data() + GUARD, h, w, 2 * pitch); break;
				default: Asm_Interpolate_Line_Interpolate(src.data(), dst.data() + GUARD, h, w, 2 * pitch); break;
			}
			t.check(c, fnv(dst.data(), dst.size()) == c.out[0]);
		}
		ok &= t.report();
	}
	{
		/*
		**	The original drew through VGA planes; the vectors hold the screen it
		**	produced (modelled under the emulator) and the source image. Where
		**	its entry ECX was clean, the screen IS the source -- which is what
		**	the native copy must produce.
		*/
		Tally t("ModeX_Blit (native copy)");
		for (Case const & c : load("modex_blit")) {
			long long const * p = &c.in[0];		// xadd pitch seed garbage_ecx
			int stride = 320 + (int)p[0] + (int)p[1];
			std::vector<unsigned char> src((size_t)stride * 200), screen(320 * 200);
			fill(src.data(), src.size(), (uint32_t)p[2]);
			WWPortView sv = {src.data(), 320, 200, (int32_t)p[0], (int32_t)p[1], 0, 0};
			WWPortView dv = {screen.data(), 320, 200, 0, 0, 0, 0};
			WWPort_ModeX_Blit(sv, dv);
			uint64_t h = fnv(screen.data(), screen.size());
			t.check(c, h == c.out[1] && (p[3] || h == c.out[0]));
		}
		ok &= t.report();
	}
	{
		Tally t("LCW_Comp (+ round trip)");
		for (Case const & c : load("lcw_comp")) {
			int size = (int)c.in[0];
			uint32_t seed = (uint32_t)c.in[1];
			std::vector<unsigned char> data = lcw_data(seed, size);
			std::vector<unsigned char> slack(128);
			fill(slack.data(), 128, seed ^ 0xFFFFFFFFu);
			data.insert(data.end(), slack.begin(), slack.end());
			std::vector<unsigned char> out(2 * size + 256), back(size + 64);
			int len = LCW_Comp(data.data(), out.data(), size);
			bool same = len == (int)c.out[0] && fnv(out.data(), len) == c.out[1];
			int got = (int)LCW_Uncompress(out.data(), back.data(), size);
			same = same && got == size && memcmp(back.data(), data.data(), size) == 0;
			t.check(c, same);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Frame_To_Page");
		for (Case const & c : load("buffer_frame_to_page")) {
			long long const * p = &c.in[0];
			TestView tv(p);
			int w = (int)p[6], h = (int)p[7], nrows = (int)p[10];
			int usebig = (int)p[11], useold = (int)p[12], theater = (int)p[13], ncalls = (int)p[14];
			std::vector<unsigned char> pixels = frame_pixels((uint32_t)p[8], w, h);
			std::vector<unsigned char> ghost, fadet, ghost2, unused;
			frame_tables((uint32_t)p[9], nrows, ghost, fadet);
			frame_tables((uint32_t)p[9] ^ 0x55555555u, nrows, ghost2, unused);

			/* one block holds the buffer base and the shape, so shape_data is a small offset */
			std::vector<unsigned char> block(64 + pixels.size() + 16);
			memcpy(block.data() + 64, pixels.data(), pixels.size());
			std::vector<unsigned char> hdr(12 + h, 0);
			uint32_t const fresh = 0xFFFFFFFFu;
			int32_t const offset = 64, th = theater;
			memcpy(&hdr[0], &fresh, 4);
			memcpy(&hdr[4], &offset, 4);
			memcpy(&hdr[8], &th, 4);
			BigShapeBufferStart = theater ? NULL : (char *)block.data();
			TheaterShapeBufferStart = theater ? (char *)block.data() : NULL;
			UseBigShapeBuffer = usebig;
			UseOldShapeDraw = useold != 0;
			void * src = (usebig && !useold) ? (void *)hdr.data() : (void *)(block.data() + 64);

			for (int k = 0; k < ncalls; k++) {
				long long const * q = p + 15 + 7 * k;		// x y flags fcount pred partial gsel
				int flags = (int)q[2];
				WWPortFrameArgs a = {q[6] ? ghost2.data() : ghost.data(), fadet.data(), (int)q[3], (int)q[4], (int)q[5]};
				WWPort_Buffer_Frame_To_Page((int)q[0], (int)q[1], w, h, src, tv.v, flags, a);
			}
			std::vector<unsigned char> hcheck(hdr.begin(), hdr.begin() + 4);
			hcheck.insert(hcheck.end(), hdr.begin() + 12, hdr.end());
			t.check(c, tv.hash() == c.out[0] && fnv(hcheck.data(), hcheck.size()) == c.out[1]);
		}
		ok &= t.report();
	}
	printf(ok ? "asm_game: all pass\n" : "asm_game: FAIL\n");
	return ok ? 0 : 1;
}
