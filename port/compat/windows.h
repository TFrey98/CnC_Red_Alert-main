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
**	The Win32 entry points the engine calls directly.
**
**	DECLARATIONS ONLY -- there is no implementation file yet. They let the tree
**	compile and let probe.sh surface the real remaining work; every one that is
**	actually reached at runtime still needs a native implementation (Cocoa,
**	CoreAudio or POSIX) behind the plain-C boundary described in
**	port/backend/ra_platform.h. Linking the game will name the ones that matter.
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
**	Additional primitive spellings the engine uses.
*/
#define VOID void
typedef unsigned char *     PBYTE;
typedef unsigned char *     LPBYTE;
typedef unsigned int *      PUINT;
typedef const void *        LPCVOID;

#define LOWORD(l)   ((WORD)((DWORD_PTR)(l) & 0xffff))
#define HIWORD(l)   ((WORD)(((DWORD_PTR)(l) >> 16) & 0xffff))
#define LOBYTE(w)   ((BYTE)((DWORD_PTR)(w) & 0xff))
#define HIBYTE(w)   ((BYTE)(((DWORD_PTR)(w) >> 8) & 0xff))
#define MAKELONG(a,b) ((LONG)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))

/*
**	Bitmap file structures. BITMAPFILEHEADER IS PACKED ON WIN32 -- it is 14
**	bytes, not 16, because bfType (2 bytes) is followed by a 4-byte field. The
**	attribute is required, not cosmetic: CODE/BMP8.CPP reads it straight off
**	disk with `sizeof(BITMAPFILEHEADER)`, so a padded 16-byte version would
**	silently misparse every .BMP.
*/
#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
	WORD  bfType;
	DWORD bfSize;
	WORD  bfReserved1;
	WORD  bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;
#pragma pack(pop)

typedef struct tagBITMAPCOREHEADER {
	DWORD bcSize;
	WORD  bcWidth;
	WORD  bcHeight;
	WORD  bcPlanes;
	WORD  bcBitCount;
} BITMAPCOREHEADER, *LPBITMAPCOREHEADER, *PBITMAPCOREHEADER;

typedef struct tagLOGPALETTE {
	WORD         palVersion;
	WORD         palNumEntries;
	PALETTEENTRY palPalEntry[1];
} LOGPALETTE, *LPLOGPALETTE;

#define BI_RGB        0
#define BI_RLE8       1
#define BI_RLE4       2
#define BI_BITFIELDS  3

typedef struct _MEMORYSTATUS {
	DWORD  dwLength;
	DWORD  dwMemoryLoad;
	SIZE_T dwTotalPhys;
	SIZE_T dwAvailPhys;
	SIZE_T dwTotalPageFile;
	SIZE_T dwAvailPageFile;
	SIZE_T dwTotalVirtual;
	SIZE_T dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;
void GlobalMemoryStatus(LPMEMORYSTATUS buffer);

typedef struct _SYSTEMTIME {
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;
void GetSystemTime(LPSYSTEMTIME time);	/* implemented in wwcompat.cpp (UTC, like Win32) */
void GetLocalTime(LPSYSTEMTIME time);	/* implemented in wwcompat.cpp */

/*
**	Window messages and the message pump.
**
**	The engine drives its own pump (CODE/KEY.CPP, KEYBOARD.CPP, WINSTUB.CPP).
**	On macOS that becomes an NSApplication run loop, so these declarations are
**	a staging post: they get the tree compiling and mark exactly which call
**	sites the Cocoa input backend has to take over.
*/
#define WM_NULL         0x0000
#define WM_CREATE       0x0001
#define WM_DESTROY      0x0002
#define WM_MOVE         0x0003
#define WM_SIZE         0x0005
#define WM_ACTIVATE     0x0006
#define WM_SETFOCUS     0x0007
#define WM_KILLFOCUS    0x0008
#define WM_PAINT        0x000F
#define WM_CLOSE        0x0010
#define WM_QUIT         0x0012
#define WM_ACTIVATEAPP  0x001C
#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_CHAR         0x0102
#define WM_SYSKEYDOWN   0x0104
#define WM_SYSKEYUP     0x0105
#define WM_COMMAND      0x0111
#define WM_TIMER        0x0113
#define WM_MOUSEMOVE    0x0200
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202
#define WM_LBUTTONDBLCLK 0x0203
#define WM_RBUTTONDOWN  0x0204
#define WM_RBUTTONUP    0x0205
#define WM_RBUTTONDBLCLK 0x0206
#define WM_USER         0x0400

#define PM_NOREMOVE     0x0000
#define PM_REMOVE       0x0001
#define PM_NOYIELD      0x0002

BOOL  GetMessageA(LPMSG msg, HWND wnd, UINT filtermin, UINT filtermax);
BOOL  PeekMessageA(LPMSG msg, HWND wnd, UINT filtermin, UINT filtermax, UINT remove);
BOOL  TranslateMessage(const MSG * msg);
LRESULT DispatchMessageA(const MSG * msg);
UINT  MapVirtualKeyA(UINT code, UINT maptype);
BOOL  SetForegroundWindow(HWND wnd);
HWND  FindWindowA(LPCSTR classname, LPCSTR windowname);
HWND  GetFocus(void);
BOOL  ShowWindow(HWND wnd, int cmdshow);
int   ShowCursor(BOOL show);		/* hides the OS cursor; the game draws its own (WWMOUSE). Native backend implements it. */

/*
**	Win32's A/W split: <windows.h> defines the unsuffixed name as a macro for
**	the ANSI variant. The engine uses the unsuffixed spellings throughout.
*/
#define GetMessage      GetMessageA
#define PeekMessage     PeekMessageA
#define DispatchMessage DispatchMessageA
#define MapVirtualKey   MapVirtualKeyA
#define FindWindow      FindWindowA

/*
**	CreateFile-style access flags. CODE/RAWFILE.CPP uses these; they map onto
**	open(2) flags in whatever implements them.
*/
#define GENERIC_READ        0x80000000u
#define GENERIC_WRITE       0x40000000u
#define FILE_SHARE_READ     0x00000001u
#define FILE_SHARE_WRITE    0x00000002u
#define CREATE_NEW          1
#define CREATE_ALWAYS       2
#define OPEN_EXISTING       3
#define OPEN_ALWAYS         4
#define TRUNCATE_EXISTING   5
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)

UINT SetErrorMode(UINT mode);

/*
**	Win32 file API, implemented for real on POSIX in wwcompat.cpp. RawFileClass
**	(CODE/RAWFILE.CPP) -- through which every MIX, INI and save file is opened --
**	is written against these. File HANDLEs are tagged file descriptors so that
**	CloseHandle() can tell them from the engine's other handle kinds.
*/
#define FILE_BEGIN               0
#define FILE_CURRENT             1
#define FILE_END                 2
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define INVALID_FILE_SIZE        ((DWORD)0xFFFFFFFF)
#define FILE_FLAG_RANDOM_ACCESS     0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN   0x08000000
#define ERROR_FILE_NOT_FOUND     2L
#define ERROR_PATH_NOT_FOUND     3L
#define ERROR_ACCESS_DENIED      5L
#define ERROR_INVALID_HANDLE     6L
#define ERROR_FILE_EXISTS        80L
#define ERROR_DISK_FULL          112L

typedef struct _FILETIME {				/* 100ns ticks since 1601-01-01 UTC */
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *LPFILETIME;

typedef struct _BY_HANDLE_FILE_INFORMATION {
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    dwVolumeSerialNumber;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
	DWORD    nNumberOfLinks;
	DWORD    nFileIndexHigh;
	DWORD    nFileIndexLow;
} BY_HANDLE_FILE_INFORMATION, *LPBY_HANDLE_FILE_INFORMATION;

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, void * security, DWORD disposition, DWORD flags, HANDLE templatefile);
BOOL   ReadFile(HANDLE file, LPVOID buffer, DWORD toread, LPDWORD read, LPOVERLAPPED overlapped);
BOOL   WriteFile(HANDLE file, LPCVOID buffer, DWORD towrite, LPDWORD written, LPOVERLAPPED overlapped);
DWORD  SetFilePointer(HANDLE file, LONG distance, LONG * distancehigh, DWORD method);
DWORD  GetFileSize(HANDLE file, LPDWORD sizehigh);
BOOL   DeleteFileA(LPCSTR name);
BOOL   GetFileInformationByHandle(HANDLE file, LPBY_HANDLE_FILE_INFORMATION info);
BOOL   SetFileTime(HANDLE file, const FILETIME * creation, const FILETIME * access, const FILETIME * write);
BOOL   FileTimeToDosDateTime(const FILETIME * filetime, WORD * dosdate, WORD * dostime);
BOOL   DosDateTimeToFileTime(WORD dosdate, WORD dostime, LPFILETIME filetime);
#define CreateFile CreateFileA
#define DeleteFile DeleteFileA
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOOPENFILEERRORBOX 0x8000

/*
**	Registry. Used only to read install paths and CD drive letters, neither of
**	which exists on macOS -- these will become NSUserDefaults or a plist, so
**	the declarations are placeholders to get the tree compiling.
*/
#define ERROR_SUCCESS       0L
#define HKEY_CLASSES_ROOT   ((HKEY)(ULONG_PTR)0x80000000)
#define HKEY_CURRENT_USER   ((HKEY)(ULONG_PTR)0x80000001)
#define HKEY_LOCAL_MACHINE  ((HKEY)(ULONG_PTR)0x80000002)
#define KEY_READ            0x20019
LONG RegCloseKey(HKEY key);
LONG RegOpenKeyExA(HKEY key, LPCSTR subkey, DWORD options, DWORD desired, PHKEY result);
LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD cbdata);
#define RegOpenKeyEx    RegOpenKeyExA
#define RegQueryValueEx RegQueryValueExA

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
static_assert(sizeof(BITMAPFILEHEADER) == 14, "BITMAPFILEHEADER must stay packed to 14 bytes");
#endif

#endif /* WWPORT_COMPAT_WINDOWS_H */
