/*
**	win32_ddraw.cpp -- DirectDraw, as the engine uses it, over the Metal
**	backend.
**
**	Engine side: built with the engine's flags. It reaches the screen only
**	through ra_platform.h.
**
**	The game draws 8-bit paletted pixels into DirectDraw surfaces (GBUFFER.CPP)
**	and shows them on a fullscreen primary surface with a hardware palette.
**	Here every surface is system memory: Lock hands out the buffer, Blt copies
**	(with colour-fill and source colour keys, the only effects the game asks
**	for), Flip swaps a primary with its attached back buffer. The primary is
**	the screen: whatever is in it when the backend presents is what is seen.
**
**	Presenting. On Windows the primary WAS the screen, so a write showed at the
**	next refresh. Here writes to the primary (Unlock, Blt, Flip) mark it dirty,
**	and the message pump presents it -- at most once per display refresh, never
**	per write: the game locks and unlocks the screen many times a frame (and
**	the mouse timer draws the cursor into it), and presenting each time would
**	stall on the display's vsync.
**
**	The palette. The primary's DirectDraw palette and the emulated VGA DAC
**	(CODE/PRAGMAUX.CPP: outportb, used by RGBClass and the movie player) both
**	feed the one displayed palette -- in 8-bit fullscreen on real hardware they
**	were the same palette too.
*/

#include "windows.h"
#include "ddraw.h"
#include "ra_platform.h"
#include "win32_internal.h"

#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_time.h>
#include <mutex>
#include <string.h>
#include <vector>

namespace {

/*
**	Debugging aid: with RA_DUMP_FRAMES=<folder> set, presented frames are also
**	written there as 8-bit .bmp files (one every RA_DUMP_EVERY presents, default
**	30), so a run can be inspected without watching it.
*/
void dump_frame(unsigned char const * pixels, int w, int h, unsigned char const * rgba)
{
	static char const * dir = getenv("RA_DUMP_FRAMES");
	static int every = getenv("RA_DUMP_EVERY") ? atoi(getenv("RA_DUMP_EVERY")) : 30;
	static int count = 0;
	if (dir == NULL || every <= 0 || (count++ % every) != 0) return;
	char name[1024];
	snprintf(name, sizeof(name), "%s/frame%05d.bmp", dir, count / every);
	FILE * f = fopen(name, "wb");
	if (f == NULL) return;
	int const row = (w + 3) & ~3;
	uint32_t const data = 14 + 40 + 1024, size = data + (uint32_t)row * h;
	unsigned char hdr[54] = {'B', 'M'};
	memcpy(hdr + 2, &size, 4); memcpy(hdr + 10, &data, 4);
	uint32_t const v40 = 40, planes_bits = 1 | (8 << 16), colors = 256;
	int32_t const iw = w, ih = h;
	memcpy(hdr + 14, &v40, 4); memcpy(hdr + 18, &iw, 4); memcpy(hdr + 22, &ih, 4);
	memcpy(hdr + 26, &planes_bits, 4); memcpy(hdr + 46, &colors, 4);
	fwrite(hdr, 1, 54, f);
	for (int i = 0; i < 256; i++) {
		unsigned char q[4] = {rgba[i * 4 + 2], rgba[i * 4 + 1], rgba[i * 4 + 0], 0};
		fwrite(q, 1, 4, f);
	}
	unsigned char pad[4] = {0, 0, 0, 0};
	for (int y = h - 1; y >= 0; y--) {
		fwrite(pixels + (size_t)y * w, 1, (size_t)w, f);
		fwrite(pad, 1, (size_t)(row - w), f);
	}
	fclose(f);
}

struct Display;
Display * TheDisplay = NULL;

/* ---------------------------------------------------------------- palette */

/*
**	The colours on screen, as the backend wants them (R, G, B, unused).
*/
unsigned char ShownRGBA[256 * 4];
std::atomic<bool> PaletteDirty(true);
std::atomic<bool> ScreenDirty(false);

struct Palette : IDirectDrawPalette {
	PALETTEENTRY	Entries[256];
	ULONG				Refs;
	bool				OnPrimary;

	Palette() : Refs(1), OnPrimary(false) {memset(Entries, 0, sizeof(Entries));}

	void show(void)
	{
		for (int i = 0; i < 256; i++) {
			ShownRGBA[i * 4 + 0] = Entries[i].peRed;
			ShownRGBA[i * 4 + 1] = Entries[i].peGreen;
			ShownRGBA[i * 4 + 2] = Entries[i].peBlue;
			ShownRGBA[i * 4 + 3] = 255;
		}
		PaletteDirty = true;
		ScreenDirty = true;
	}

	HRESULT GetEntries(DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY out)
	{
		(void)flags;
		if (out == NULL || start + count > 256) return DDERR_INVALIDPARAMS;
		memcpy(out, Entries + start, count * sizeof(PALETTEENTRY));
		return DD_OK;
	}
	HRESULT SetEntries(DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY in)
	{
		(void)flags;
		if (in == NULL || start + count > 256) return DDERR_INVALIDPARAMS;
		memcpy(Entries + start, in, count * sizeof(PALETTEENTRY));
		if (OnPrimary) show();
		return DD_OK;
	}
	ULONG Release()
	{
		ULONG r = --Refs;
		if (r == 0) delete this;
		return r;
	}
};

/* --------------------------------------------------------------- surfaces */

struct Surface : IDirectDrawSurface {
	std::vector<unsigned char>	Pixels;
	int								Width, Height;
	DWORD								Caps;
	Palette *						Pal;
	Surface *						Back;				// attached back buffer, for Flip
	DDCOLORKEY						SrcKey;
	bool								HasSrcKey;
	ULONG								Refs;

	Surface(int w, int h, DWORD caps) : Pixels((size_t)w * h, 0), Width(w), Height(h), Caps(caps),
		Pal(NULL), Back(NULL), HasSrcKey(false), Refs(1)
	{
		SrcKey.dwColorSpaceLowValue = SrcKey.dwColorSpaceHighValue = 0;
	}

	bool primary(void) const {return (Caps & DDSCAPS_PRIMARYSURFACE) != 0;}
	void touched(void) {if (primary()) ScreenDirty = true;}

	/* NULL means the whole surface; otherwise clipped to it. False if empty. */
	bool area(LPRECT r, RECT & out) const
	{
		if (r == NULL) {out.left = 0; out.top = 0; out.right = Width; out.bottom = Height;}
		else out = *r;
		if (out.left < 0) out.left = 0;
		if (out.top < 0) out.top = 0;
		if (out.right > Width) out.right = Width;
		if (out.bottom > Height) out.bottom = Height;
		return out.right > out.left && out.bottom > out.top;
	}

	void describe(LPDDSURFACEDESC d)
	{
		DWORD size = d->dwSize;
		memset(d, 0, sizeof(*d));
		d->dwSize = size ? size : sizeof(*d);
		d->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT;
		d->dwWidth = (DWORD)Width;
		d->dwHeight = (DWORD)Height;
		d->lPitch = Width;
		d->ddsCaps.dwCaps = Caps;
		d->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
		d->ddpfPixelFormat.dwRGBBitCount = 8;
	}

	/*
	**	Copy (stretching by nearest pixel if the rectangles differ in size),
	**	skipping source pixels equal to the source colour key when asked.
	*/
	HRESULT Blt(LPRECT destrect, LPDIRECTDRAWSURFACE srcsurf, LPRECT srcrect, DWORD flags, LPDDBLTFX fx)
	{
		RECT d;
		if (!area(destrect, d)) return DD_OK;
		if (flags & DDBLT_COLORFILL) {
			unsigned char c = fx ? (unsigned char)fx->dwFillColor : 0;
			for (LONG y = d.top; y < d.bottom; y++) memset(&Pixels[(size_t)y * Width + d.left], c, (size_t)(d.right - d.left));
			touched();
			return DD_OK;
		}
		Surface * src = (Surface *)srcsurf;
		if (src == NULL) return DDERR_INVALIDPARAMS;
		RECT s;
		if (!src->area(srcrect, s)) return DD_OK;
		bool const keyed = (flags & DDBLT_KEYSRC) && src->HasSrcKey;
		unsigned char const key = (unsigned char)src->SrcKey.dwColorSpaceLowValue;
		LONG const dw = d.right - d.left, dh = d.bottom - d.top;
		LONG const sw = s.right - s.left, sh = s.bottom - s.top;
		std::vector<unsigned char> copy;
		unsigned char const * from = &src->Pixels[0];
		if (src == this) {copy = Pixels; from = &copy[0];}		// overlapping self-blit
		for (LONG y = 0; y < dh; y++) {
			unsigned char const * sl = from + (size_t)(s.top + y * sh / dh) * src->Width + s.left;
			unsigned char * dl = &Pixels[(size_t)(d.top + y) * Width + d.left];
			if (dw == sw && !keyed) {
				memcpy(dl, sl, (size_t)dw);
				continue;
			}
			for (LONG x = 0; x < dw; x++) {
				unsigned char p = sl[x * sw / dw];
				if (!keyed || p != key) dl[x] = p;
			}
		}
		touched();
		return DD_OK;
	}

	HRESULT BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE src, LPRECT srcrect, DWORD trans)
	{
		Surface * s = (Surface *)src;
		if (s == NULL) return DDERR_INVALIDPARAMS;
		RECT sr;
		if (!s->area(srcrect, sr)) return DD_OK;
		RECT dr = {(LONG)x, (LONG)y, (LONG)x + (sr.right - sr.left), (LONG)y + (sr.bottom - sr.top)};
		return Blt(&dr, src, &sr, (trans & 1) ? DDBLT_KEYSRC : 0, NULL);	// DDBLTFAST_SRCCOLORKEY
	}

	/* The primary and its back buffer trade contents, as page flipping did. */
	HRESULT Flip(LPDIRECTDRAWSURFACE target, DWORD flags)
	{
		(void)flags;
		Surface * other = target ? (Surface *)target : Back;
		if (other == NULL || other->Pixels.size() != Pixels.size()) return DDERR_INVALIDPARAMS;
		Pixels.swap(other->Pixels);
		touched();
		return DD_OK;
	}

	HRESULT GetCaps(LPDDSCAPS caps) {if (caps) caps->dwCaps = Caps; return DD_OK;}
	HRESULT GetSurfaceDesc(LPDDSURFACEDESC desc) {if (desc) describe(desc); return DD_OK;}
	HRESULT IsLost() {return DD_OK;}
	HRESULT GetBltStatus(DWORD flags) {(void)flags; return DD_OK;}
	HRESULT Restore() {return DD_OK;}

	HRESULT Lock(LPRECT rect, LPDDSURFACEDESC desc, DWORD flags, HANDLE event)
	{
		(void)flags; (void)event;
		if (desc == NULL) return DDERR_INVALIDPARAMS;
		describe(desc);
		size_t offset = rect ? (size_t)rect->top * Width + rect->left : 0;
		desc->lpSurface = &Pixels[0] + offset;
		desc->dwFlags |= DDSD_LPSURFACE;
		return DD_OK;
	}
	HRESULT Unlock(LPVOID data) {(void)data; touched(); return DD_OK;}

	HRESULT SetPalette(LPDIRECTDRAWPALETTE p)
	{
		Palette * pal = (Palette *)p;
		if (Pal == pal) return DD_OK;
		if (Pal) {Pal->OnPrimary = false; Pal->Release();}
		Pal = pal;
		if (Pal) {
			Pal->Refs++;
			if (primary()) {Pal->OnPrimary = true; Pal->show();}
		}
		return DD_OK;
	}
	HRESULT GetPalette(LPDIRECTDRAWPALETTE * p)
	{
		if (p == NULL) return DDERR_INVALIDPARAMS;
		*p = Pal;
		if (Pal == NULL) return DDERR_NOPALETTEATTACHED;
		Pal->Refs++;
		return DD_OK;
	}
	HRESULT SetColorKey(DWORD flags, LPDDCOLORKEY key)
	{
		(void)flags;
		HasSrcKey = key != NULL;
		if (key) SrcKey = *key;
		return DD_OK;
	}
	HRESULT AddAttachedSurface(LPDIRECTDRAWSURFACE attach) {Back = (Surface *)attach; return DD_OK;}

	ULONG Release()
	{
		ULONG r = --Refs;
		if (r == 0) {
			if (TheDisplay != NULL) detach(this);
			if (Pal) {Pal->OnPrimary = false; Pal->Release();}
			delete this;
		}
		return r;
	}
	static void detach(Surface * s);
};

/* ------------------------------------------------------------ DirectDraw */

struct Display : IDirectDraw2 {
	int			Width, Height;
	Surface *	Primary;
	ULONG			Refs;
	std::mutex	PresentLock;
	uint64_t		LastPresent;

	Display() : Width(640), Height(480), Primary(NULL), Refs(1), LastPresent(0) {}

	HRESULT CreateSurface(LPDDSURFACEDESC desc, LPDIRECTDRAWSURFACE * out, void * outer)
	{
		(void)outer;
		if (desc == NULL || out == NULL) return DDERR_INVALIDPARAMS;
		DWORD caps = (desc->dwFlags & DDSD_CAPS) ? desc->ddsCaps.dwCaps : DDSCAPS_OFFSCREENPLAIN;
		int w = Width, h = Height;
		if (!(caps & DDSCAPS_PRIMARYSURFACE)) {
			if (desc->dwFlags & DDSD_WIDTH) w = (int)desc->dwWidth;
			if (desc->dwFlags & DDSD_HEIGHT) h = (int)desc->dwHeight;
		}
		if (w <= 0 || h <= 0) return DDERR_INVALIDPARAMS;
		Surface * s = new Surface(w, h, caps);
		if (caps & DDSCAPS_PRIMARYSURFACE) {
			std::lock_guard<std::mutex> g(PresentLock);
			Primary = s;
		}
		*out = s;
		return DD_OK;
	}
	HRESULT CreatePalette(DWORD caps, LPPALETTEENTRY entries, LPDIRECTDRAWPALETTE * out, void * outer)
	{
		(void)caps; (void)outer;
		if (out == NULL) return DDERR_INVALIDPARAMS;
		Palette * p = new Palette;
		if (entries) memcpy(p->Entries, entries, sizeof(p->Entries));
		*out = p;
		return DD_OK;
	}
	HRESULT SetCooperativeLevel(HWND window, DWORD flags) {(void)window; (void)flags; return DD_OK;}

	/*
	**	The framebuffer becomes this size; the Mac window rescales to fit. Only
	**	8 bits a pixel is drawn. The 16-bit request is the DVD edition's MPEG
	**	movie path, which this port does not take (PORTSTUB.CPP).
	*/
	HRESULT SetDisplayMode(DWORD width, DWORD height, DWORD bpp)
	{
		(void)bpp;
		Width = (int)width;
		Height = (int)height;
		RA_Display_Resize(WWPort_Main_Display(), Width, Height);
		return DD_OK;
	}
	HRESULT RestoreDisplayMode() {return DD_OK;}
	HRESULT GetCaps(LPDDCAPS driver, LPDDCAPS emulation)
	{
		LPDDCAPS both[2] = {driver, emulation};
		for (LPDDCAPS c : both) {
			if (c == NULL) continue;
			DWORD size = c->dwSize;
			memset(c, 0, sizeof(*c));
			c->dwSize = size;
			c->dwCaps = DDCAPS_BLT | DDCAPS_BLTCOLORFILL | DDCAPS_COLORKEY;
			c->dwVidMemTotal = c->dwVidMemFree = 64u * 1024 * 1024;
		}
		return DD_OK;
	}
	HRESULT FlipToGDISurface() {return DD_OK;}

	/*
	**	DDRAW.CPP's Wait_Vert_Blank, for pacing palette changes. The backend
	**	presents on the display's refresh; this presents anything pending now.
	*/
	HRESULT WaitForVerticalBlank(DWORD flags, HANDLE event)
	{
		(void)flags; (void)event;
		present(true);
		return DD_OK;
	}
	HRESULT GetAvailableVidMem(LPDDSCAPS caps, LPDWORD total, LPDWORD freemem)
	{
		(void)caps;
		if (total) *total = 64u * 1024 * 1024;
		if (freemem) *freemem = 64u * 1024 * 1024;
		return DD_OK;
	}
	ULONG Release()
	{
		ULONG r = --Refs;
		if (r == 0) {
			if (TheDisplay == this) TheDisplay = NULL;
			delete this;
		}
		return r;
	}

	/*
	**	Show the primary if it changed -- no more than once per 1/60 s unless
	**	`now`. Main thread only (the backend is Cocoa).
	*/
	void present(bool now)
	{
		if (!ScreenDirty && !PaletteDirty) return;
		static mach_timebase_info_data_t tb;
		if (tb.denom == 0) mach_timebase_info(&tb);
		uint64_t t = mach_absolute_time() * tb.numer / tb.denom;
		if (!now && t - LastPresent < 16666666ull) return;
		RA_Display * display = WWPort_Main_Display();
		std::lock_guard<std::mutex> g(PresentLock);
		if (display == NULL || Primary == NULL) return;
		if (Primary->Width != Width || Primary->Height != Height) return;
		LastPresent = t;
		if (PaletteDirty.exchange(false)) RA_Display_SetPalette(display, ShownRGBA);
		ScreenDirty = false;
		RA_Display_Present(display, &Primary->Pixels[0]);
		dump_frame(&Primary->Pixels[0], Width, Height, ShownRGBA);
	}
};

void Surface::detach(Surface * s)
{
	std::lock_guard<std::mutex> g(TheDisplay->PresentLock);
	if (TheDisplay->Primary == s) TheDisplay->Primary = NULL;
}

}

extern "C" HRESULT DirectDrawCreate(void * guid, LPDIRECTDRAW * dd, void * outer)
{
	(void)guid; (void)outer;
	if (dd == NULL) return DDERR_INVALIDPARAMS;
	if (TheDisplay == NULL) TheDisplay = new Display;
	else TheDisplay->Refs++;
	*dd = TheDisplay;
	return DD_OK;
}

/*
**	Called from the message pump (win32_window.cpp).
*/
void WWPort_Display_Pump(void)
{
	if (TheDisplay) TheDisplay->present(false);
}

/*
**	The emulated VGA DAC changed one colour (6-bit components): the screen's
**	palette follows, as it did on the real hardware.
*/
extern "C" void WWPort_DAC_Entry(int index, int red, int green, int blue)
{
	index &= 0xFF;
	ShownRGBA[index * 4 + 0] = (unsigned char)((red & 63) << 2);
	ShownRGBA[index * 4 + 1] = (unsigned char)((green & 63) << 2);
	ShownRGBA[index * 4 + 2] = (unsigned char)((blue & 63) << 2);
	ShownRGBA[index * 4 + 3] = 255;
	PaletteDirty = true;
}
