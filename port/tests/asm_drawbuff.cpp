/*
**	asm_drawbuff.cpp -- the C translations of WIN32LIB/DRAWBUFF's assembly vs
**	vectors recorded from the original assembly (port/asmref).
*/
#include "asm_replay.h"
#include "gbuffer.h"
#include "tile.h"
#include "iconcach.h"
#include "stampvar.h"

void WWPort_Buffer_Draw_Stamp(WWPortView const & v, void const * icondata, int icon, int x, int y, unsigned char const * remap);
void WWPort_Buffer_Draw_Stamp_Clip(WWPortView const & v, void const * icondata, int icon, int x, int y,
	unsigned char const * remap, int min_x, int min_y, int max_x, int max_y);

/*
**	Stand-ins for ICONCACH.CPP, scripted exactly as gen_vectors.py's stubs.
*/
extern "C" {
IconSetType	IconSetList[MAX_ICON_SETS];
short			IconCacheLookup[MAX_LOOKUP_ENTRIES];
}
static struct {
	long long slots[3], ok[3];
	int n;
	unsigned char const * ic;
	std::vector<long long> log;
} Script;
extern "C" int Get_Free_Cache_Slot(void)
{
	return (int)Script.slots[Script.n++ % 3];
}
extern "C" BOOL Cache_New_Icon(int icon_index, void * icon_ptr)
{
	Script.log.push_back(icon_index);
	Script.log.push_back((unsigned char *)icon_ptr - Script.ic);
	return (BOOL)Script.ok[(Script.log.size() / 2) % 3];
}

void WWPort_Buffer_Clear(WWPortView const & v, unsigned char color);
void WWPort_Buffer_Put_Pixel(WWPortView const & v, int x, int y, unsigned char color);
int  WWPort_Buffer_Get_Pixel(WWPortView const & v, int x, int y);
void WWPort_Buffer_Fill_Rect(WWPortView const & v, int sx, int sy, int dx, int dy, unsigned char color);
void WWPort_Buffer_Remap(WWPortView const & v, int x, int y, int width, int height, unsigned char const * remap);
long WWPort_Buffer_To_Buffer(WWPortView const & v, int x, int y, int w, int h, void * buff, long size);
void WWPort_Buffer_Draw_Line(WWPortView const & v, int sx, int sy, int dx, int dy, unsigned char color);
int WWPort_Linear_Blit_To_Linear(WWPortView const & src, WWPortView const & dst, int x, int y,
	int dx, int dy, int pixel_width, int pixel_height, int trans);
int WWPort_Linear_Scale_To_Linear(WWPortView const & src, WWPortView const & dst, int src_x, int src_y,
	int dst_x, int dst_y, int src_width, int src_height, int dst_width, int dst_height,
	int trans, unsigned char const * remap);
long WWPort_Buffer_To_Page(int x, int y, int w, int h, void const * buff, WWPortView const & v);

int main()
{
	bool ok = true;
	{
		Tally t("Buffer_Clear");
		for (Case const & c : load("buffer_clear")) {
			TestView tv(&c.in[0]);
			WWPort_Buffer_Clear(tv.v, (unsigned char)c.in[6]);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Put_Pixel");
		for (Case const & c : load("buffer_put_pixel")) {
			TestView tv(&c.in[0]);
			WWPort_Buffer_Put_Pixel(tv.v, (int)c.in[6], (int)c.in[7], (unsigned char)c.in[8]);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Get_Pixel");
		for (Case const & c : load("buffer_get_pixel")) {
			TestView tv(&c.in[0]);
			int r = WWPort_Buffer_Get_Pixel(tv.v, (int)c.in[6], (int)c.in[7]);
			t.check(c, r == (int)(long long)c.out[0] && tv.hash() == c.out[1]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Fill_Rect");
		for (Case const & c : load("buffer_fill_rect")) {
			TestView tv(&c.in[0]);
			WWPort_Buffer_Fill_Rect(tv.v, (int)c.in[6], (int)c.in[7], (int)c.in[8], (int)c.in[9], (unsigned char)c.in[10]);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Remap");
		for (Case const & c : load("buffer_remap")) {
			TestView tv(&c.in[0]);
			unsigned char table[256];
			fill(table, 256, (uint32_t)c.in[10]);
			WWPort_Buffer_Remap(tv.v, (int)c.in[6], (int)c.in[7], (int)c.in[8], (int)c.in[9], c.in[10] ? table : NULL);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_To_Buffer");
		for (Case const & c : load("buffer_to_buffer")) {
			TestView tv(&c.in[0]);
			TestLinear buf(c.in[8], c.in[9], (uint32_t)c.in[10]);
			long r = WWPort_Buffer_To_Buffer(tv.v, (int)c.in[6], (int)c.in[7], (int)c.in[8], (int)c.in[9], buf.addr(), (long)c.in[11]);
			t.check(c, (int32_t)r == (int32_t)(long long)c.out[0] && tv.hash() == c.out[1] && buf.hash() == c.out[2]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_To_Page");
		for (Case const & c : load("buffer_to_page")) {
			TestView tv(&c.in[0]);
			TestLinear buf(c.in[8], c.in[9], (uint32_t)c.in[10]);
			WWPort_Buffer_To_Page((int)c.in[6], (int)c.in[7], (int)c.in[8], (int)c.in[9], c.in[11] ? buf.addr() : NULL, tv.v);
			t.check(c, tv.hash() == c.out[0] && buf.hash() == c.out[1]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Draw_Line");
		for (Case const & c : load("buffer_draw_line")) {
			TestView tv(&c.in[0]);
			WWPort_Buffer_Draw_Line(tv.v, (int)c.in[6], (int)c.in[7], (int)c.in[8], (int)c.in[9], (unsigned char)c.in[10]);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Linear_Blit_To_Linear");
		for (Case const & c : load("linear_blit_to_linear")) {
			long long const * p = &c.in[0];
			TestBlock a(p[0], p[1], (uint32_t)p[2]);
			TestBlock other(p[4], p[5], (uint32_t)p[6]);
			TestBlock & b = p[3] ? a : other;
			WWPortView sv = a.view(p[7], p[8], p[9], p[10]);
			WWPortView dv = b.view(p[11], p[12], p[13], p[14]);
			int r = WWPort_Linear_Blit_To_Linear(sv, dv, (int)p[15], (int)p[16], (int)p[17], (int)p[18], (int)p[19], (int)p[20], (int)p[21]);
			t.check(c, r == (int32_t)(long long)c.out[0] && a.hash() == c.out[1] && (p[3] || b.hash() == c.out[2]));
		}
		ok &= t.report();
	}
	{
		Tally t("Linear_Scale_To_Linear");
		for (Case const & c : load("linear_scale_to_linear")) {
			long long const * p = &c.in[0];
			TestBlock a(p[0], p[1], (uint32_t)p[2]);
			TestBlock other(p[4], p[5], (uint32_t)p[6]);
			TestBlock & b = p[3] ? a : other;
			WWPortView sv = a.view(p[7], p[8], p[9], p[10]);
			WWPortView dv = b.view(p[11], p[12], p[13], p[14]);
			unsigned char table[256];
			fill(table, 256, (uint32_t)p[24]);
			WWPort_Linear_Scale_To_Linear(sv, dv, (int)p[15], (int)p[16], (int)p[17], (int)p[18], (int)p[19], (int)p[20],
				(int)p[21], (int)p[22], (int)p[23], p[24] ? table : NULL);
			t.check(c, a.hash() == c.out[0] && (p[3] || b.hash() == c.out[1]));
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Draw_Stamp_Clip");
		for (Case const & c : load("buffer_draw_stamp_clip")) {
			long long const * p = &c.in[0];
			TestView tv(p);
			std::vector<unsigned char> ic = make_iconset((int)p[6], (int)p[7], (int)p[8], (uint32_t)p[9]);
			unsigned char table[256];
			fill(table, 256, (uint32_t)p[13]);
			LastIconset = NULL;
			WWPort_Buffer_Draw_Stamp_Clip(tv.v, ic.data(), (int)p[10], (int)p[11], (int)p[12], p[13] ? table : NULL,
				(int)p[14], (int)p[15], (int)p[16], (int)p[17]);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Buffer_Draw_Stamp");
		for (Case const & c : load("buffer_draw_stamp")) {
			long long const * p = &c.in[0];
			TestView tv(p);
			std::vector<unsigned char> ic = make_iconset((int)p[6], (int)p[7], (int)p[8], (uint32_t)p[9]);
			unsigned char table[256];
			fill(table, 256, (uint32_t)p[13]);
			LastIconset = NULL;
			WWPort_Buffer_Draw_Stamp(tv.v, ic.data(), (int)p[10], (int)p[11], (int)p[12], p[13] ? table : NULL);
			t.check(c, tv.hash() == c.out[0]);
		}
		ok &= t.report();
	}
	{
		Tally t("Is_Icon_Cached");
		for (Case const & c : load("is_icon_cached")) {
			long long const * p = &c.in[0];		// cnt iseed regidx listoff lseed icon slots[3] ok[3]
			std::vector<unsigned char> ic = make_iconset(24, 24, (int)p[0], (uint32_t)p[1]);
			unsigned char * lookup = (unsigned char *)IconCacheLookup;
			fill(lookup, 6000, (uint32_t)p[4]);
			for (int i = 0; i < 6000; i += 2) if (lookup[i] & 1) lookup[i] = lookup[i + 1] = 0xFF;
			memset(IconSetList, 0, sizeof(IconSetList));
			if (p[2] >= 0) {
				IconSetList[p[2]].IconSetPtr = (IControl_Type *)ic.data();
				IconSetList[p[2]].IconListOffset = (int)p[3];
			}
			for (int i = 0; i < 3; i++) {Script.slots[i] = p[6 + i]; Script.ok[i] = p[9 + i];}
			Script.n = 0; Script.ic = ic.data(); Script.log.clear();
			LastIconset = NULL;
			int r = Is_Icon_Cached(ic.data(), (int)p[5]);
			bool same = r == (int32_t)(long long)c.out[0] && fnv(lookup, 6000) == c.out[1]
				&& Script.log.size() / 2 == c.out[2];
			for (size_t i = 0; same && i < Script.log.size(); i++) same = Script.log[i] == (long long)c.out[3 + i];
			t.check(c, same);
		}
		ok &= t.report();
	}
	{
		Tally t("Cache_Copy_Icon");
		for (Case const & c : load("cache_copy_icon")) {
			long long const * p = &c.in[0];
			unsigned char src[576];
			fill(src, 576, (uint32_t)p[0]);
			std::vector<unsigned char> dst(GUARD + p[1] * 24 + GUARD);
			fill(dst.data(), dst.size(), (uint32_t)p[2]);
			Cache_Copy_Icon(src, dst.data() + GUARD, (int)p[1]);
			t.check(c, fnv(dst.data(), dst.size()) == c.out[0]);
		}
		ok &= t.report();
	}
	printf(ok ? "asm_drawbuff: all pass\n" : "asm_drawbuff: FAIL\n");
	return ok ? 0 : 1;
}
