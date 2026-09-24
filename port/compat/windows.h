/*
**	windows.h -- Win32 compatibility shim for the native macOS (Apple Silicon) port.
**
**	The original Red Alert sources are ILP32 and assume the Win32 SDK. macOS on
**	arm64 is LP64, so the fixed-width Win32 integer types are pinned to `int`
**	rather than `long` here. Getting this wrong silently changes the layout of
**	every serialized struct (saved games, MIX headers, network packets), so the
**	static assertions at the bottom of this file enforce it at compile time.
**
**	This header intentionally provides types and declarations only. Behaviour
**	lives in port/compat/wincompat.cpp, backed by SDL2 where a real
**	implementation is required.
*/
#ifndef WWPORT_COMPAT_WINDOWS_H
#define WWPORT_COMPAT_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
**	Calling conventions and memory-model keywords. arm64 has exactly one calling
**	convention, and no segmented memory, so these all evaporate.
*/
#define __cdecl
#define __stdcall
#define __fastcall
#define WINAPI
#define APIENTRY
#define CALLBACK
#define PASCAL
#define FAR
#define NEAR
#define far
#define near
#define _far
#define _near
#define __far
#define __near
#define __export
#define __based(x)

/*
**	Fixed-width integer types. DWORD/LONG/UINT are 32 bits on Win32 and must
**	stay 32 bits here -- see the note at the top of this file.
*/
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef int                 LONG;
typedef unsigned int        ULONG;
typedef unsigned int        UINT;
typedef int                 INT;
typedef short               SHORT;
typedef unsigned short      USHORT;
typedef char                CHAR;
typedef unsigned char       UCHAR;
typedef int                 BOOL;

typedef BYTE *              LPBYTE;
typedef WORD *              LPWORD;
typedef DWORD *             LPDWORD;
typedef LONG *              LPLONG;
typedef int *               LPINT;
typedef char *              LPSTR;
typedef const char *        LPCSTR;
typedef void *              LPVOID;
typedef const void *        LPCVOID;
typedef char *              LPTSTR;
typedef const char *        LPCTSTR;

/*
**	Pointer-width types. These MUST follow the pointer, not DWORD, or 64-bit
**	handle and message values get truncated.
*/
typedef intptr_t            LPARAM;
typedef uintptr_t           WPARAM;
typedef intptr_t            LRESULT;
typedef size_t              SIZE_T;
typedef uintptr_t           ULONG_PTR;
typedef intptr_t            LONG_PTR;
typedef uintptr_t           DWORD_PTR;

/*
**	Opaque handles. Distinct struct pointers rather than a shared void* so the
**	compiler still catches handle-type mixups that the original build caught.
*/
#define WWPORT_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name
WWPORT_DECLARE_HANDLE(HWND);
WWPORT_DECLARE_HANDLE(HDC);
WWPORT_DECLARE_HANDLE(HBITMAP);
WWPORT_DECLARE_HANDLE(HPALETTE);
WWPORT_DECLARE_HANDLE(HMENU);
WWPORT_DECLARE_HANDLE(HICON);
WWPORT_DECLARE_HANDLE(HCURSOR);
WWPORT_DECLARE_HANDLE(HBRUSH);
WWPORT_DECLARE_HANDLE(HFONT);
WWPORT_DECLARE_HANDLE(HKEY);
WWPORT_DECLARE_HANDLE(HRGN);

typedef void *              HANDLE;
typedef void *              HINSTANCE;
typedef void *              HMODULE;
typedef void *              HGLOBAL;
typedef void *              HLOCAL;
typedef HANDLE *            LPHANDLE;
typedef HKEY *              PHKEY;

/*
**	min/max.
**
**	Real <windows.h> defines these as macros unless NOMINMAX is set, and the
**	engine uses them that way in ~37 files (AIRCRAFT.CPP, ANIM.CPP,
**	BUILDING.CPP, BULLET.CPP and on).
**
**	Macros, not templates, and that distinction matters: many call sites mix
**	argument types -- `min(scatterdist, Rule.HomingScatter)` in BULLET.CPP pairs
**	an int with a fixed-point member. A `template<class T> T min(T,T)` would
**	fail to deduce T for those; the macro never cared, which is precisely why
**	the original code could be written this way.
**
**	The engine's own Min/Max templates in WIN32LIB/INCLUDE/WWSTD.H are separate
**	and stay as they are -- capitalised, so there is no collision.
**
**	These are the standard double-evaluation macros, identical to Win32's. That
**	is a real hazard with side-effecting arguments, but it is the behaviour the
**	original build had, so matching it keeps the port faithful rather than
**	quietly changing evaluation counts. Only <new> is included tree-wide, so
**	there is no <algorithm> for these to break.
*/
#ifndef NOMINMAX
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#endif

/*
**	SOCKET.
**
**	Real <windows.h> pulls in <winsock.h> unless WIN32_LEAN_AND_MEAN is set, and
**	CODE/tcpip.h depends on that: it declares `void Close_Socket(SOCKET s);`
**	without including anything itself.
**
**	Only the typedef is mirrored here, not the whole BSD socket surface --
**	compat/winsock.h still owns that, and dragging <sys/socket.h> into all 277
**	translation units would invite collisions for no benefit. The typedef is
**	identical to the one there, so including both is well formed.
*/
#ifndef WWPORT_SOCKET_DEFINED
#define WWPORT_SOCKET_DEFINED
typedef int                 SOCKET;
#define INVALID_SOCKET      (-1)
#define SOCKET_ERROR        (-1)
#endif

/*
**	DDE (Dynamic Data Exchange) handles.
**
**	On Win32 these come from <ddeml.h>, which <windows.h> pulls in; CODE/dde.h
**	and CCDDE.* have no #includes of their own and rely on exactly that. They
**	are opaque handles there and are only ever passed around here, so distinct
**	handle types are enough -- no DDEML implementation is implied.
*/
WWPORT_DECLARE_HANDLE(HSZ);       /* DDE string handle       */
WWPORT_DECLARE_HANDLE(HDDEDATA);  /* DDE data handle         */
WWPORT_DECLARE_HANDLE(HCONV);     /* DDE conversation handle */

/*
**	OVERLAPPED -- Win32 asynchronous I/O control block.
**
**	WIN32LIB/INCLUDE/wincomm.h embeds two of these BY VALUE (ReadOverlap,
**	WriteOverlap), so unlike PORT in commlib.h this one cannot be left opaque;
**	the struct has to be laid out.
**
**	The field order and widths below are the real Win32 ones, so sizeof() and
**	offsets match what the serial code expects. hEvent is the only member that
**	code actually touches. Note this is a *layout*, not a mechanism: nothing on
**	macOS services these. The serial port path that uses them is out of scope
**	(see compat/modem.h); if it is ever revived, kqueue or a reader thread
**	replaces the semantics, not just the struct.
*/
typedef struct _OVERLAPPED {
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	DWORD     Offset;
	DWORD     OffsetHigh;
	HANDLE    hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef LONG                HRESULT;
typedef DWORD               COLORREF;
typedef int (*FARPROC)(void);
typedef int (*PROC)(void);

#define S_OK            ((HRESULT)0)
#define S_FALSE         ((HRESULT)1)
#define E_FAIL          ((HRESULT)0x80004005)
#define E_INVALIDARG    ((HRESULT)0x80070057)
#define E_OUTOFMEMORY   ((HRESULT)0x8007000E)
#define E_NOINTERFACE   ((HRESULT)0x80004002)
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

/*
**	Geometry and palette structures. Layout is load-bearing: BITMAPINFOHEADER
**	and PALETTEENTRY are written to and read from disk by the shape and palette
**	code, so field order and widths match the Win32 SDK exactly.
*/
typedef struct tagRECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, *LPRECT, *PRECT;

typedef struct tagPOINT {
	LONG x;
	LONG y;
} POINT, *LPPOINT, *PPOINT;

typedef struct tagSIZE {
	LONG cx;
	LONG cy;
} SIZE, *LPSIZE;

typedef struct tagPALETTEENTRY {
	BYTE peRed;
	BYTE peGreen;
	BYTE peBlue;
	BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

typedef struct tagRGBQUAD {
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
} RGBQUAD;

#pragma pack(push, 2)
typedef struct tagBITMAPINFOHEADER {
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;

typedef struct tagBITMAPINFO {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD          bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
#pragma pack(pop)

typedef struct tagMSG {
	HWND   hwnd;
	UINT   message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD  time;
	POINT  pt;
} MSG, *LPMSG;

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);

/*
**	Threading. Win32 critical sections are recursive, so the pthread mutex
**	backing them is initialised recursive to match -- the mouse and audio code
**	both re-enter their section from the same thread.
*/
#include <pthread.h>

typedef struct _RTL_CRITICAL_SECTION {
	pthread_mutex_t mutex;
	int             initialized;
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

#ifdef __cplusplus
extern "C" {
#endif

void  InitializeCriticalSection(LPCRITICAL_SECTION section);
void  DeleteCriticalSection(LPCRITICAL_SECTION section);
void  EnterCriticalSection(LPCRITICAL_SECTION section);
void  LeaveCriticalSection(LPCRITICAL_SECTION section);

/*
**	The handful of Win32 entry points the engine calls directly. Backed by
**	SDL2/POSIX in port/compat/wincompat.cpp.
*/
DWORD   GetTickCount(void);
void    Sleep(DWORD milliseconds);
DWORD   GetLastError(void);
void    SetLastError(DWORD error);
BOOL    CloseHandle(HANDLE object);
void    OutputDebugStringA(LPCSTR text);
int     MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type);
short   GetAsyncKeyState(int key);
short   GetKeyState(int key);

/*
**	GlobalAlloc/GlobalLock date from 16-bit Windows' movable memory blocks.
**	On a flat 64-bit address space allocation is just malloc and locking is a
**	no-op that returns the same pointer.
*/
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
LPVOID  GlobalLock(HGLOBAL mem);
BOOL    GlobalUnlock(HGLOBAL mem);
HGLOBAL GlobalFree(HGLOBAL mem);

#ifdef __cplusplus
}
#endif

#define OutputDebugString OutputDebugStringA
#define MessageBox        MessageBoxA
#define GMEM_FIXED        0x0000
#define GMEM_MOVEABLE     0x0002
#define GMEM_ZEROINIT     0x0040
#define GPTR              (GMEM_FIXED | GMEM_ZEROINIT)
#define MB_OK             0x00000000
#define MB_ICONERROR      0x00000010
#define FILE_ATTRIBUTE_NORMAL 0x00000080

/*
**	Compile-time guarantees that the LP64 host did not widen anything.
*/
#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(BYTE)  == 1, "BYTE must be 8 bits");
static_assert(sizeof(WORD)  == 2, "WORD must be 16 bits");
static_assert(sizeof(DWORD) == 4, "DWORD must be 32 bits on LP64");
static_assert(sizeof(LONG)  == 4, "LONG must be 32 bits on LP64");
static_assert(sizeof(PALETTEENTRY) == 4, "PALETTEENTRY layout changed");
static_assert(sizeof(BITMAPINFOHEADER) == 40, "BITMAPINFOHEADER layout changed");
#endif

#endif /* WWPORT_COMPAT_WINDOWS_H */
