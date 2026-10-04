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
**	Result codes, numbered exactly as the DirectX SDK does: MAKE_DDHRESULT(code).
**	Only the port's own backend ever produces them, so the values matter for
**	diagnostics, not behaviour -- but WIN32LIB/MISC/DDRAW.CPP switches over
**	nearly all of them to print error text, and a duplicate would surface there as
**	a duplicate case label. Generating them from one macro also fixed two earlier
**	hand-typed values: WASSTILLDRAWING (was 0x8760021C) and SURFACEBUSY (was
**	0x887601AA, i.e. code 426 rather than 430).
*/
#define MAKE_DDHRESULT(code)        ((HRESULT)(0x88760000u | (unsigned)(code)))
#define DD_OK                       ((HRESULT)0)
#define DDERR_GENERIC               ((HRESULT)0x80004005)	/* = E_FAIL */
#define DDERR_INVALIDPARAMS         ((HRESULT)0x80070057)	/* = E_INVALIDARG */
#define DDERR_OUTOFMEMORY           ((HRESULT)0x8007000E)	/* = E_OUTOFMEMORY */
#define DDERR_UNSUPPORTED           ((HRESULT)0x80004001)	/* = E_NOTIMPL */
#define DDERR_ALREADYINITIALIZED             MAKE_DDHRESULT(5)
#define DDERR_CANNOTATTACHSURFACE            MAKE_DDHRESULT(10)
#define DDERR_CANNOTDETACHSURFACE            MAKE_DDHRESULT(20)
#define DDERR_CURRENTLYNOTAVAIL              MAKE_DDHRESULT(40)
#define DDERR_EXCEPTION                      MAKE_DDHRESULT(55)
#define DDERR_HEIGHTALIGN                    MAKE_DDHRESULT(90)
#define DDERR_INCOMPATIBLEPRIMARY            MAKE_DDHRESULT(95)
#define DDERR_INVALIDCAPS                    MAKE_DDHRESULT(100)
#define DDERR_INVALIDCLIPLIST                MAKE_DDHRESULT(110)
#define DDERR_INVALIDMODE                    MAKE_DDHRESULT(120)
#define DDERR_INVALIDOBJECT                  MAKE_DDHRESULT(130)
#define DDERR_INVALIDPIXELFORMAT             MAKE_DDHRESULT(145)
#define DDERR_INVALIDRECT                    MAKE_DDHRESULT(150)
#define DDERR_LOCKEDSURFACES                 MAKE_DDHRESULT(160)
#define DDERR_NO3D                           MAKE_DDHRESULT(170)
#define DDERR_NOALPHAHW                      MAKE_DDHRESULT(180)
#define DDERR_NOCLIPLIST                     MAKE_DDHRESULT(205)
#define DDERR_NOCOLORCONVHW                  MAKE_DDHRESULT(210)
#define DDERR_NOCOOPERATIVELEVELSET          MAKE_DDHRESULT(212)
#define DDERR_NOCOLORKEY                     MAKE_DDHRESULT(215)
#define DDERR_NOCOLORKEYHW                   MAKE_DDHRESULT(220)
#define DDERR_NODIRECTDRAWSUPPORT            MAKE_DDHRESULT(222)
#define DDERR_NOEXCLUSIVEMODE                MAKE_DDHRESULT(225)
#define DDERR_NOFLIPHW                       MAKE_DDHRESULT(230)
#define DDERR_NOGDI                          MAKE_DDHRESULT(240)
#define DDERR_NOMIRRORHW                     MAKE_DDHRESULT(250)
#define DDERR_NOTFOUND                       MAKE_DDHRESULT(255)
#define DDERR_NOOVERLAYHW                    MAKE_DDHRESULT(260)
#define DDERR_NORASTEROPHW                   MAKE_DDHRESULT(280)
#define DDERR_NOROTATIONHW                   MAKE_DDHRESULT(290)
#define DDERR_NOSTRETCHHW                    MAKE_DDHRESULT(310)
#define DDERR_NOT4BITCOLOR                   MAKE_DDHRESULT(316)
#define DDERR_NOT4BITCOLORINDEX              MAKE_DDHRESULT(317)
#define DDERR_NOT8BITCOLOR                   MAKE_DDHRESULT(320)
#define DDERR_NOTEXTUREHW                    MAKE_DDHRESULT(330)
#define DDERR_NOVSYNCHW                      MAKE_DDHRESULT(335)
#define DDERR_NOZBUFFERHW                    MAKE_DDHRESULT(340)
#define DDERR_NOZOVERLAYHW                   MAKE_DDHRESULT(350)
#define DDERR_OUTOFCAPS                      MAKE_DDHRESULT(360)
#define DDERR_OUTOFVIDEOMEMORY               MAKE_DDHRESULT(380)
#define DDERR_OVERLAYCANTCLIP                MAKE_DDHRESULT(382)
#define DDERR_OVERLAYCOLORKEYONLYONEACTIVE   MAKE_DDHRESULT(384)
#define DDERR_PALETTEBUSY                    MAKE_DDHRESULT(387)
#define DDERR_COLORKEYNOTSET                 MAKE_DDHRESULT(400)
#define DDERR_SURFACEALREADYATTACHED         MAKE_DDHRESULT(410)
#define DDERR_SURFACEALREADYDEPENDENT        MAKE_DDHRESULT(420)
#define DDERR_SURFACEBUSY                    MAKE_DDHRESULT(430)
#define DDERR_CANTLOCKSURFACE                MAKE_DDHRESULT(435)
#define DDERR_SURFACEISOBSCURED              MAKE_DDHRESULT(440)
#define DDERR_SURFACELOST                    MAKE_DDHRESULT(450)
#define DDERR_SURFACENOTATTACHED             MAKE_DDHRESULT(460)
#define DDERR_TOOBIGHEIGHT                   MAKE_DDHRESULT(470)
#define DDERR_TOOBIGSIZE                     MAKE_DDHRESULT(480)
#define DDERR_TOOBIGWIDTH                    MAKE_DDHRESULT(490)
#define DDERR_UNSUPPORTEDFORMAT              MAKE_DDHRESULT(510)
#define DDERR_UNSUPPORTEDMASK                MAKE_DDHRESULT(520)
#define DDERR_VERTICALBLANKINPROGRESS        MAKE_DDHRESULT(537)
#define DDERR_WASSTILLDRAWING                MAKE_DDHRESULT(540)
#define DDERR_XALIGN                         MAKE_DDHRESULT(560)
#define DDERR_INVALIDDIRECTDRAWGUID          MAKE_DDHRESULT(561)
#define DDERR_DIRECTDRAWALREADYCREATED       MAKE_DDHRESULT(562)
#define DDERR_NODIRECTDRAWHW                 MAKE_DDHRESULT(563)
#define DDERR_PRIMARYSURFACEALREADYEXISTS    MAKE_DDHRESULT(564)
#define DDERR_NOEMULATION                    MAKE_DDHRESULT(565)
#define DDERR_REGIONTOOSMALL                 MAKE_DDHRESULT(566)
#define DDERR_CLIPPERISUSINGHWND             MAKE_DDHRESULT(567)
#define DDERR_NOCLIPPERATTACHED              MAKE_DDHRESULT(568)
#define DDERR_NOHWND                         MAKE_DDHRESULT(569)
#define DDERR_HWNDSUBCLASSED                 MAKE_DDHRESULT(570)
#define DDERR_HWNDALREADYSET                 MAKE_DDHRESULT(571)
#define DDERR_NOPALETTEATTACHED              MAKE_DDHRESULT(572)
#define DDERR_NOPALETTEHW                    MAKE_DDHRESULT(573)
#define DDERR_BLTFASTCANTCLIP                MAKE_DDHRESULT(574)
#define DDERR_NOBLTHW                        MAKE_DDHRESULT(575)
#define DDERR_NODDROPSHW                     MAKE_DDHRESULT(576)
#define DDERR_OVERLAYNOTVISIBLE              MAKE_DDHRESULT(577)
#define DDERR_NOOVERLAYDEST                  MAKE_DDHRESULT(578)
#define DDERR_INVALIDPOSITION                MAKE_DDHRESULT(579)
#define DDERR_NOTAOVERLAYSURFACE             MAKE_DDHRESULT(580)
#define DDERR_EXCLUSIVEMODEALREADYSET        MAKE_DDHRESULT(581)
#define DDERR_NOTFLIPPABLE                   MAKE_DDHRESULT(582)
#define DDERR_CANTDUPLICATE                  MAKE_DDHRESULT(583)
#define DDERR_NOTLOCKED                      MAKE_DDHRESULT(584)
#define DDERR_CANTCREATEDC                   MAKE_DDHRESULT(585)
#define DDERR_NODC                           MAKE_DDHRESULT(586)
#define DDERR_WRONGMODE                      MAKE_DDHRESULT(587)
#define DDERR_IMPLICITLYCREATED              MAKE_DDHRESULT(588)
#define DDERR_NOTPALETTIZED                  MAKE_DDHRESULT(589)
#define DDERR_UNSUPPORTEDMODE                MAKE_DDHRESULT(590)
#define DDERR_NOMIPMAPHW                     MAKE_DDHRESULT(591)
#define DDERR_INVALIDSURFACETYPE             MAKE_DDHRESULT(592)

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
#define DDCAPS_BLTQUEUE             0x00000080
#define DDCAPS_PALETTEVSYNC         0x00010000
#define DDCAPS_NOHARDWARE           0x02000000
#define DDCAPS_BANKSWITCHED         0x08000000
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

#define DDWAITVB_BLOCKBEGIN         0x00000001
#define DDWAITVB_BLOCKBEGINEVENT    0x00000002
#define DDWAITVB_BLOCKEND           0x00000004

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
	virtual HRESULT GetPalette(LPDIRECTDRAWPALETTE * palette) = 0;	/* CONQUER.CPP reads back the attached palette */
	virtual HRESULT SetColorKey(DWORD flags, LPDDCOLORKEY colorkey) = 0;
	/* GBUFFER.CPP attaches the back buffer to the primary for page flipping. */
	virtual HRESULT AddAttachedSurface(LPDIRECTDRAWSURFACE attach) = 0;
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
	/* Frame pacing (DDRAW.CPP's Wait_Vert_Blank). The Metal backend paces on the display link. */
	virtual HRESULT WaitForVerticalBlank(DWORD flags, HANDLE event) = 0;
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
