/*
**	asm_misc.cpp -- C translations of WIN32LIB's small MISC/MEM/FONT/SHAPE
**	assembly vs vectors recorded from the original assembly (port/asmref).
*/
#include "asm_replay.h"
#include "wwstd.h"
#include "misc.h"
#include "font.h"
#include "shape.h"
#include "palette.h"
#include "wsa.h"
#include "soundint.h"
#include "soscomp.h"
#include "gbuffer.h"
#include "mouse.h"

extern "C" int __cdecl LCW_Comp(void const * source, void * dest, int datasize);
void WWPort_Mouse_Shadow_Buffer(int cursor_width, int cursor_height, WWPortView const & v, unsigned char * buffer,
	int x, int y, int hotx, int hoty, int store);
void WWPort_Draw_Mouse(unsigned char const * cursor, int cursor_width, int cursor_height, int xhot, int yhot,
	WWPortView const & v, int x, int y);
struct WWPortMouseFields {
	unsigned char * MouseCursor;
	int MouseXHot, MouseYHot, CursorWidth, CursorHeight, MaxWidth, MaxHeight;
	void * PrevCursor;
};
void * WWPort_Set_Mouse_Cursor(WWPortMouseFields & m, int xhotspot, int yhotspot, void * cursor);

/* gen_vectors.py's cursor_shape(): header (+colours), then RLE data. */
static void cursor_shape(uint32_t seed, int w, int h, int stype, std::vector<unsigned char> & hdr, std::vector<unsigned char> & rle)
{
	std::vector<unsigned char> px((size_t)w * h);
	fill(px.data(), px.size(), seed);
	rle.clear();
	for (size_t i = 0; i < px.size();) {
		if (px[i] % 4 == 0) {
			int run = 1;
			while (i + run < px.size() && px[i + run] % 4 == 0 && run < 255) run++;
			rle.push_back(0); rle.push_back((unsigned char)run);
			i += run;
		} else {
			rle.push_back((stype & 1) ? (unsigned char)(px[i] % 15 + 1) : px[i]);
			i++;
		}
	}
	unsigned char const h10[10] = {(unsigned char)stype, (unsigned char)(stype >> 8), (unsigned char)h,
		(unsigned char)w, (unsigned char)(w >> 8), (unsigned char)h, 0, 0, (unsigned char)rle.size(), (unsigned char)(rle.size() >> 8)};
	hdr.assign(h10, h10 + 10);
	if (stype & 1) {
		unsigned char colors[16];
		fill(colors, 16, seed ^ 0x77777777u);
		hdr.insert(hdr.end(), colors, colors + 16);
	}
}

/* Stand-in for the display layer (WIN32LIB/MISC/DDRAW.CPP): records the call. */
static void * DDPaletteSeen;
static int DDPaletteCalls;
extern "C" void Set_DD_Palette(void * palette) {DDPaletteSeen = palette; DDPaletteCalls++;}

/* gen_vectors.py's palette(). */
static void make_palette(unsigned char * pal, uint32_t seed)
{
	fill(pal, 768, seed);
	if (seed & 3) for (int i = 0; i < 768; i++) pal[i] &= 63;
}

extern "C" unsigned char ColorXlat[256];
unsigned char ColorXlat[256];		// CODE/2TXTPRNT's table, stood in for here
extern "C" void __cdecl Mem_Copy(void const * source, void * dest, unsigned long bytes_to_copy);

int main()
{
	bool ok = true;
	{
		Tally t("Clip_Rect");
		for (Case const & c : load("clip_rect")) {
			int v[4] = {(int)c.in[0], (int)c.in[1], (int)c.in[2], (int)c.in[3]};
			int r = Clip_Rect(&v[0], &v[1], &v[2], &v[3], (int)c.in[4], (int)c.in[5]);
			bool same = r == (int32_t)(long long)c.out[0];
			for (int i = 0; i < 4; i++) same = same && v[i] == (int32_t)(long long)c.out[1 + i];
			t.check(c, same);
		}
		ok &= t.report();
	}
	{
		Tally t("Confine_Rect");
		for (Case const & c : load("confine_rect")) {
			int x = (int)c.in[0], y = (int)c.in[1];
			int r = Confine_Rect(&x, &y, (int)c.in[2], (int)c.in[3], (int)c.in[4], (int)c.in[5]);
			t.check(c, r == (int32_t)(long long)c.out[0] && x == (int32_t)(long long)c.out[1] && y == (int32_t)(long long)c.out[2]);
		}
		ok &= t.report();
	}
	{
		Tally t("Reverse_Long/Short, Swap_Long");
		for (Case const & c : load("reverse")) {
			long v = (long)(int32_t)(uint32_t)c.in[0];
			bool same = (uint32_t)Reverse_Long(v) == (uint32_t)c.out[0]
				&& (uint16_t)Reverse_Short((short)v) == (uint16_t)c.out[1]
				&& (uint32_t)Swap_Long(v) == (uint32_t)c.out[2]
				&& Reverse_Long(v) == (long)(int32_t)(uint32_t)c.out[0];		// sign-extended, as a Watcom long
			t.check(c, same);
		}
		ok &= t.report();
	}
	{
		Tally t("Mem_Copy");
		for (Case const & c : load("mem_copy")) {
			long long const * p = &c.in[0];		// size seed so do count nulls
			std::vector<unsigned char> buf(p[0]);
			fill(buf.data(), buf.size(), (uint32_t)p[1]);
			Mem_Copy(p[5] == 1 ? NULL : buf.data() + p[2], p[5] == 2 ? NULL : buf.data() + p[3], (unsigned long)p[4]);
			t.check(c, fnv(buf.data(), buf.size()) == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Set_Font_Palette_Range");
		for (Case const & c : load("set_font_palette_range")) {
			unsigned char pal[16];
			fill(ColorXlat, 256, (uint32_t)c.in[0]);
			fill(pal, 16, (uint32_t)c.in[1]);
			Set_Font_Palette_Range(pal, (int)c.in[2], (int)c.in[3]);
			t.check(c, fnv(ColorXlat, 256) == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Set_Shape_Buffer");
		static char buffer[100];
		Set_Shape_Buffer(buffer, 1234);
		Case c; c.line = 0;
		t.check(c, _ShapeBuffer == buffer && _ShapeBufferSize == 1234);
		ok &= t.report();
	}
	{
		Tally t("Build_Fading_Table");
		for (Case const & c : load("build_fading_table")) {
			unsigned char pal[768];
			make_palette(pal, (uint32_t)c.in[0]);
			unsigned char dest[256 + 2 * GUARD];
			fill(dest, sizeof(dest), (uint32_t)c.in[1]);
			void * r = Build_Fading_Table(pal, dest + GUARD, (long)c.in[2], (long)c.in[3]);
			t.check(c, (r == dest + GUARD) == (bool)c.out[0] && fnv(dest, sizeof(dest)) == c.out[1]);
		}
		ok &= t.report();
	}
	{
		Tally t("Bump_Color");
		for (Case const & c : load("bump_color")) {
			unsigned char pal[768];
			make_palette(pal, (uint32_t)c.in[0]);
			BOOL r = Bump_Color(pal, (int)c.in[1], (int)c.in[2]);
			t.check(c, (unsigned long long)r == c.out[0] && fnv(pal, 768) == c.out[1]);
		}
		ok &= t.report();
	}
	{
		Tally t("Set_Palette_Range");
		for (Case const & c : load("set_palette_range")) {
			unsigned char pal[768];
			make_palette(pal, (uint32_t)c.in[0]);
			bool same = true;
			if (c.in[1]) same = fnv(CurrentPalette, 768) == c.out[0];		// the initial all-255 state
			DDPaletteCalls = 0; DDPaletteSeen = NULL;
			Set_Palette_Range(pal);
			same = same && fnv(CurrentPalette, 768) == c.out[1] && (DDPaletteCalls == 1 && DDPaletteSeen == pal) == (bool)c.out[2];
			t.check(c, same);
		}
		ok &= t.report();
	}
	{
		Tally t("Apply_XOR_Delta");
		for (Case const & c : load("apply_xor_delta")) {
			long long const * p = &c.in[0];		// size seed len delta...
			std::vector<unsigned char> buf(p[0] + 2 * GUARD), delta(p[2]);
			fill(buf.data(), buf.size(), (uint32_t)p[1]);
			for (long long i = 0; i < p[2]; i++) delta[i] = (unsigned char)p[3 + i];
			unsigned r = Apply_XOR_Delta((char *)buf.data() + GUARD, (char *)delta.data());
			t.check(c, r == c.out[0] && fnv(buf.data(), buf.size()) == c.out[1]);
		}
		ok &= t.report();
	}
	{
		Tally t("Apply_XOR_Delta_To_Page_Or_Viewport");
		for (Case const & c : load("apply_xor_delta_to_page_or_viewport")) {
			long long const * p = &c.in[0];		// width rows nextrow seed copy len delta...
			std::vector<unsigned char> buf(p[2] * p[1] + 2 * GUARD), delta(p[5]);
			fill(buf.data(), buf.size(), (uint32_t)p[3]);
			for (long long i = 0; i < p[5]; i++) delta[i] = (unsigned char)p[6 + i];
			Apply_XOR_Delta_To_Page_Or_Viewport(buf.data() + GUARD, delta.data(), (int)p[0], (int)p[2], (int)p[4]);
			t.check(c, fnv(buf.data(), buf.size()) == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Decompress_Frame");
		for (Case const & c : load("decompress_frame")) {
			t.check(c, snd1_case(c, [](void * s, void * d, long n) {return Decompress_Frame(s, d, n);}));
		}
		ok &= t.report();
	}
	{
		Tally t("General_sosCODECDecompressData");
		for (Case const & c : load("general_sos")) {
			t.check(c, sos_case<_SOS_COMPRESS_INFO>(c, sosCODECInitStream, General_sosCODECDecompressData, true));
		}
		ok &= t.report();
	}
	{
		Tally t("sosCODECDecompressData (16-bit)");
		for (Case const & c : load("sos16")) {
			t.check(c, sos_case<_SOS_COMPRESS_INFO>(c, sosCODECInitStream, sosCODECDecompressData, false));
		}
		ok &= t.report();
	}
	{
		Tally t("Mouse_Shadow_Buffer");
		for (Case const & c : load("mouse_shadow_buffer")) {
			long long const * p = &c.in[0];
			TestView tv(p);
			int cw = (int)p[6], ch = (int)p[7];
			std::vector<unsigned char> buf(GUARD + cw * ch + GUARD);
			fill(buf.data(), buf.size(), (uint32_t)p[8]);
			WWPort_Mouse_Shadow_Buffer(cw, ch, tv.v, buf.data() + GUARD, (int)p[9], (int)p[10], (int)p[11], (int)p[12], (int)p[13]);
			t.check(c, tv.hash() == c.out[0] && fnv(buf.data(), buf.size()) == c.out[1]);
		}
		ok &= t.report();
	}
	{
		Tally t("Draw_Mouse");
		for (Case const & c : load("draw_mouse")) {
			long long const * p = &c.in[0];
			TestView tv(p);
			int cw = (int)p[6], ch = (int)p[7];
			std::vector<unsigned char> cur((size_t)cw * ch);
			fill(cur.data(), cur.size(), (uint32_t)p[8]);
			for (size_t i = 0; i < cur.size(); i++) if (cur[i] & 1) cur[i] = 0;
			WWPort_Draw_Mouse(cur.data(), cw, ch, (int)p[9], (int)p[10], tv.v, (int)p[11], (int)p[12]);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("ASM_Set_Mouse_Cursor");
		static char shape_buffer[4096];
		for (Case const & c : load("set_mouse_cursor")) {
			long long const * p = &c.in[0];		// stype maxw maxh w h seed cseed prev hx hy
			int stype = (int)p[0], maxw = (int)p[1], maxh = (int)p[2];
			std::vector<unsigned char> hdr, rle, shape;
			cursor_shape((uint32_t)p[5], (int)p[3], (int)p[4], stype, hdr, rle);
			shape = hdr;
			if (stype & 2) {
				shape.insert(shape.end(), rle.begin(), rle.end());
			} else {
				std::vector<unsigned char> src(rle), packed(2 * rle.size() + 256);
				src.resize(rle.size() + 128, 0);
				int n = LCW_Comp(src.data(), packed.data(), (int)rle.size());
				shape.insert(shape.end(), packed.begin(), packed.begin() + n);
			}
			shape.resize(shape.size() + 64, 0);
			_ShapeBuffer = shape_buffer;
			std::vector<unsigned char> cur(maxw * maxh + 64);
			fill(cur.data(), cur.size(), (uint32_t)p[6]);
			void * prev = (void *)(uintptr_t)(uint32_t)p[7];
			WWPortMouseFields f = {cur.data(), 7, 9, 3, 4, maxw, maxh, prev};
			void * r = WWPort_Set_Mouse_Cursor(f, (int)p[8], (int)p[9], shape.data());
			bool same = (r == prev) == (bool)c.out[0] && (f.PrevCursor == shape.data()) == (bool)c.out[1]
				&& f.MouseXHot == (int)(long long)c.out[2] && f.MouseYHot == (int)(long long)c.out[3]
				&& f.CursorWidth == (int)(long long)c.out[4] && f.CursorHeight == (int)(long long)c.out[5]
				&& fnv(cur.data(), cur.size()) == c.out[6];
			t.check(c, same);
		}
		ok &= t.report();
	}
	printf(ok ? "asm_misc: all pass\n" : "asm_misc: FAIL\n");
	return ok ? 0 : 1;
}
