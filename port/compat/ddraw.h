/*
**	ddraw.h -- DirectDraw compatibility shim for the native macOS port.
**
**	Red Alert talks to DirectDraw through a narrow slice of the API: create a
**	primary and a couple of offscreen surfaces, lock them to get a raw 8-bit
**	pointer, blit rectangles between them, and manage a 256-entry palette.
**	Everything else in DirectDraw goes unused.
**
**	That slice maps cleanly onto SDL2, so these interfaces are declared as C++
**	abstract classes matching the original virtual-call style
**	(`surface->Lock(...)`). The concrete SDL2-backed implementations live in
**	port/compat/ddraw_sdl.cpp, which keeps the rest of the engine unaware that
**	DirectDraw is gone.
*/
#ifndef WWPORT_COMPAT_DDRAW_H
#define WWPORT_COMPAT_DDRAW_H

#include "windows.h"

/*
**	Result codes. DirectDraw's HRESULTs are only ever compared against DD_OK
**	and DDERR_SURFACELOST by this codebase.
*/
#define DD_OK                       ((HRESULT)0)
#define DDERR_GENERIC               ((HRESULT)0x80004005)
#define DDERR_INVALIDPARAMS         ((HRESULT)0x80070057)
#define DDERR_OUTOFMEMORY           ((HRESULT)0x8007000E)
#define DDERR_SURFACELOST           ((HRESULT)0x887601C2)
#define DDERR_SURFACEBUSY           ((HRESULT)0x887601AA)
#define DDERR_WASSTILLDRAWING       ((HRESULT)0x8760021C)
#define DDERR_UNSUPPORTED           ((HRESULT)0x80004001)

/* Surface description validity flags. */
#define DDSD_CAPS                   0x00000001
#define DDSD_HEIGHT                 0x00000002
#define DDSD_WIDTH                  0x00000004
#define DDSD_PITCH                  0x00000008
#define DDSD_BACKBUFFERCOUNT        0x00000020
#define DDSD_PIXELFORMAT            0x00001000
#define DDSD_LPSURFACE              0x00000800
#define DDSD_ALL                    0x000ff9ee

/* Surface capability flags. */
#define DDSCAPS_PRIMARYSURFACE      0x00000200
#define DDSCAPS_OFFSCREENPLAIN      0x00000040
#define DDSCAPS_SYSTEMMEMORY        0x00000800
#define DDSCAPS_VIDEOMEMORY         0x00004000
#define DDSCAPS_BACKBUFFER          0x00000004
#define DDSCAPS_FLIP                0x00000010
#define DDSCAPS_MODEX               0x00200000
#define DDSCAPS_PALETTE             0x00000100

/* Blit flags. Only WAIT/ASYNC/KEYSRC/COLORFILL are referenced. */
#define DDBLT_WAIT                  0x01000000
#define DDBLT_ASYNC                 0x00000200
#define DDBLT_KEYSRC                0x00008000
#define DDBLT_COLORFILL             0x00000400
#define DDBLT_KEYDEST               0x00002000

/* Lock flags. */
#define DDLOCK_WAIT                 0x00000001
#define DDLOCK_SURFACEMEMORYPTR     0x00000000
#define DDLOCK_READONLY             0x00000010
#define DDLOCK_WRITEONLY            0x00000020

/* Palette capability flags. */
#define DDPCAPS_8BIT                0x00000004
#define DDPCAPS_ALLOW256            0x00000040
#define DDPCAPS_INITIALIZE          0x00000008
#define DDPCAPS_PRIMARYSURFACE      0x00000010

/* Driver capability flags. */
#define DDCAPS_BLT                  0x00000040
#define DDCAPS_BLTCOLORFILL         0x04000000
#define DDCAPS_COLORKEY             0x00400000

/* Blit-status query flags, used by the page-flip wait loop. */
#define DDGBS_CANBLT                0x00000001
#define DDGBS_ISBLTDONE             0x00000002

/* Cooperative level flags. */
#define DDSCL_FULLSCREEN            0x00000001
#define DDSCL_ALLOWREBOOT           0x00000002
#define DDSCL_NOWINDOWCHANGES       0x00000004
#define DDSCL_NORMAL                0x00000008
#define DDSCL_EXCLUSIVE             0x00000010
#define DDSCL_ALLOWMODEX            0x00000040

struct IDirectDraw;
struct IDirectDraw2;
struct IDirectDrawSurface;
struct IDirectDrawPalette;

typedef struct IDirectDraw *        LPDIRECTDRAW;
typedef struct IDirectDraw2 *       LPDIRECTDRAW2;
typedef struct IDirectDrawSurface * LPDIRECTDRAWSURFACE;
typedef struct IDirectDrawPalette * LPDIRECTDRAWPALETTE;

typedef struct _DDSCAPS {
	DWORD dwCaps;
} DDSCAPS, *LPDDSCAPS;

typedef struct _DDCOLORKEY {
	DWORD dwColorSpaceLowValue;
	DWORD dwColorSpaceHighValue;
} DDCOLORKEY, *LPDDCOLORKEY;

typedef struct _DDPIXELFORMAT {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwRGBAlphaBitMask;
} DDPIXELFORMAT, *LPDDPIXELFORMAT;

typedef struct _DDSURFACEDESC {
	DWORD         dwSize;
	DWORD         dwFlags;
	DWORD         dwHeight;
	DWORD         dwWidth;
	LONG          lPitch;
	DWORD         dwBackBufferCount;
	DWORD         dwRefreshRate;
	DWORD         dwAlphaBitDepth;
	DWORD         dwReserved;
	LPVOID        lpSurface;
	DDCOLORKEY    ddckCKDestOverlay;
	DDCOLORKEY    ddckCKDestBlt;
	DDCOLORKEY    ddckCKSrcOverlay;
	DDCOLORKEY    ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS       ddsCaps;
} DDSURFACEDESC, *LPDDSURFACEDESC;

typedef struct _DDBLTFX {
	DWORD dwSize;
	DWORD dwDDFX;
	DWORD dwROP;
	DWORD dwDDROP;
	DWORD dwRotationAngle;
	DWORD dwZBufferOpCode;
	DWORD dwFillColor;
	DDCOLORKEY ddckDestColorkey;
	DDCOLORKEY ddckSrcColorkey;
} DDBLTFX, *LPDDBLTFX;

typedef struct _DDCAPS {
	DWORD dwSize;
	DWORD dwCaps;
	DWORD dwCaps2;
	DWORD dwVidMemTotal;
	DWORD dwVidMemFree;
	DWORD dwMaxVisibleOverlays;
	DWORD dwNumFourCCCodes;
} DDCAPS, *LPDDCAPS;

/*
**	The engine only ever calls these methods. Anything DirectDraw offered beyond
**	this list is deliberately absent rather than stubbed, so that an unported
**	call site fails loudly at compile time instead of silently at runtime.
*/
struct IDirectDrawPalette {
	virtual HRESULT GetEntries(DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY entries) = 0;
	virtual HRESULT SetEntries(DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY entries) = 0;
	virtual ULONG   Release() = 0;
protected:
	~IDirectDrawPalette() {}
};

struct IDirectDrawSurface {
	virtual HRESULT Blt(LPRECT destrect, LPDIRECTDRAWSURFACE src, LPRECT srcrect, DWORD flags, LPDDBLTFX fx) = 0;
	virtual HRESULT BltFast(DWORD x, DWORD y, LPDIRECTDRAWSURFACE src, LPRECT srcrect, DWORD trans) = 0;
	virtual HRESULT Flip(LPDIRECTDRAWSURFACE target, DWORD flags) = 0;
	virtual HRESULT GetCaps(LPDDSCAPS caps) = 0;
	virtual HRESULT GetSurfaceDesc(LPDDSURFACEDESC desc) = 0;
	virtual HRESULT IsLost() = 0;
	virtual HRESULT GetBltStatus(DWORD flags) = 0;
	virtual HRESULT Lock(LPRECT rect, LPDDSURFACEDESC desc, DWORD flags, HANDLE event) = 0;
	virtual HRESULT Unlock(LPVOID surfacedata) = 0;
	virtual HRESULT Restore() = 0;
	virtual HRESULT SetPalette(LPDIRECTDRAWPALETTE palette) = 0;
	virtual HRESULT SetColorKey(DWORD flags, LPDDCOLORKEY colorkey) = 0;
	virtual ULONG   Release() = 0;
protected:
	~IDirectDrawSurface() {}
};

struct IDirectDraw {
	virtual HRESULT CreateSurface(LPDDSURFACEDESC desc, LPDIRECTDRAWSURFACE * surface, void * outer) = 0;
	virtual HRESULT CreatePalette(DWORD caps, LPPALETTEENTRY entries, LPDIRECTDRAWPALETTE * palette, void * outer) = 0;
	virtual HRESULT SetCooperativeLevel(HWND window, DWORD flags) = 0;
	virtual HRESULT SetDisplayMode(DWORD width, DWORD height, DWORD bpp) = 0;
	virtual HRESULT RestoreDisplayMode() = 0;
	virtual HRESULT GetCaps(LPDDCAPS driver, LPDDCAPS emulation) = 0;
	virtual HRESULT FlipToGDISurface() = 0;
	virtual ULONG   Release() = 0;
protected:
	~IDirectDraw() {}
};

/* IDirectDraw2 adds only GetAvailableVidMem in the paths this game uses. */
struct IDirectDraw2 : public IDirectDraw {
	virtual HRESULT GetAvailableVidMem(LPDDSCAPS caps, LPDWORD total, LPDWORD free) = 0;
protected:
	~IDirectDraw2() {}
};

extern "C" HRESULT DirectDrawCreate(void * guid, LPDIRECTDRAW * dd, void * outer);

#endif /* WWPORT_COMPAT_DDRAW_H */
