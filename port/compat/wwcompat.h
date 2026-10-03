/*
**	wwcompat.h -- prologue force-included into every translation unit
**	(clang -include port/compat/wwcompat.h).
**
**	Supplies the non-standard Watcom/Borland CRT entry points the Westwood code
**	relies on. Keeping them here rather than editing the 300-odd original source
**	files keeps the port's diff against EA's release small and reviewable.
*/
#ifndef WWPORT_COMPAT_WWCOMPAT_H
#define WWPORT_COMPAT_WWCOMPAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdint.h>

/*
**	Calling-convention keywords, in their bare and single-underscore spellings.
**
**	arm64 has one calling convention, so these all evaporate. windows.h defines
**	the double-underscore forms, but these belong here instead: CODE/WWALLOC.H
**	and CODE/CDFILE.CPP use bare `cdecl` as a declaration modifier without
**	necessarily having included windows.h first, and this prologue is
**	force-included into every translation unit.
**
**	Guarded because CODE/MEMCHECK.H also defines `cdecl` and `_cdecl` empty
**	under its own conditionals.
*/
#ifndef cdecl
#define cdecl
#endif
#ifndef _cdecl
#define _cdecl
#endif
#ifndef _stdcall
#define _stdcall
#endif
#ifndef _fastcall
#define _fastcall
#endif
#ifndef _pascal
#define _pascal
#endif

/*
**	Endianness.
**
**	The engine tests `#ifdef BIG_ENDIAN` to pick the field order of the unions
**	that overlay COORDINATE, CELL, TARGET, LEPTON and fixed-point values on
**	their component bitfields. That test is WRONG on any BSD-derived system:
**	<machine/endian.h> defines BIG_ENDIAN unconditionally as the constant 4321,
**	as the *name of an order* to compare BYTE_ORDER against -- not as a claim
**	about this machine. macOS pulls that header in transitively, so plain
**	`#ifdef` sees it defined and selects the big-endian layout on a
**	little-endian arm64 target, silently reversing the byte and bitfield order
**	of the engine's most fundamental types.
**
**	RA_BIG_ENDIAN is the honest test. The six `#ifdef BIG_ENDIAN` sites in
**	CODE/ (DEFINES.H, FIXED.H, BASE64.CPP) were changed to `#if RA_BIG_ENDIAN`
**	to use it. It is deliberately *not* named BIG_ENDIAN, so it cannot collide
**	with the system constant.
*/
#if defined(__BIG_ENDIAN__) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define RA_BIG_ENDIAN 1
#else
#define RA_BIG_ENDIAN 0
#endif

/*
**	Case-insensitive comparison. macOS spells these without the leading 'str'
**	prefix variants Watcom used.
*/
#define stricmp   strcasecmp
#define strnicmp  strncasecmp
#define _stricmp  strcasecmp
#define _strnicmp strncasecmp
#define memicmp   wwport_memicmp
#define _memicmp  wwport_memicmp

/*
**	Westwood declares `random(unsigned long mod)`, which collides with POSIX
**	`random(void)` from <stdlib.h>. The game never calls the POSIX one, so the
**	Westwood spelling is renamed engine-wide. <stdlib.h> is included above this
**	point so the system declaration itself is left untouched.
*/
#define random ww_random

/*
**	Bit rotates. On arm64 clang lowers these to a single ROR instruction, so the
**	original hand-written x86 rotate had no advantage worth preserving.
*/
static inline unsigned int wwport_rotl32(unsigned int v, int c)
{
	c &= 31;
	return c ? ((v << c) | (v >> (32 - c))) : v;
}
static inline unsigned int wwport_rotr32(unsigned int v, int c)
{
	c &= 31;
	return c ? ((v >> c) | (v << (32 - c))) : v;
}
#define _lrotl(v, c) wwport_rotl32((unsigned int)(v), (int)(c))
#define _lrotr(v, c) wwport_rotr32((unsigned int)(v), (int)(c))
/* _rotl/_rotr are NOT defined here: CODE/jshell.h supplies its own template
** versions, and a macro would shadow them. Only the _lrotl/_lrotr spellings,
** which the engine uses but never defines, are provided. */

/*
**	ww_lvalue(temporary) -- pass a temporary where the engine takes a non-const
**	reference, as in `ini.Load(CCFileClass("RULES.INI"))`.
**
**	Watcom (like old MSVC) bound temporaries to non-const references; standard
**	C++ does not. Naming the temporary instead would change its lifetime -- a
**	CCFileClass would keep its file open to the end of the enclosing block rather
**	than closing at the end of the statement. This returns a reference that is
**	valid for exactly the full-expression, which is the lifetime it had before.
*/
#ifdef __cplusplus
template<class T> static inline T & ww_lvalue(T && temporary) { return temporary; }
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
**	Integer-to-string. The Watcom contract is that the caller supplies the
**	buffer and the same pointer comes back, which snprintf does not provide.
*/
char * itoa(int value, char * buffer, int radix);
char * ltoa(long value, char * buffer, int radix);
char * ultoa(unsigned long value, char * buffer, int radix);

/* In-place case conversion and reversal; return the same buffer. */
char * strupr(char * string);
char * strlwr(char * string);
char * strrev(char * string);

int wwport_memicmp(const void * a, const void * b, size_t count);

/*
**	DOS/Win32 path-component limits, from Watcom's <stdlib.h>.
**
**	These are not merely buffer hints -- the engine declares struct members with
**	them (`char Suffix[_MAX_EXT]` in HouseTypeClass, `char GraphicName[_MAX_FNAME]`
**	in ObjectTypeClass), so their values decide those classes' layout.
**
**	The values below are Watcom's **Win32** set, which is also MSVC's. Watcom's
**	DOS set is far smaller (_MAX_FNAME 9, _MAX_EXT 5, for 8.3 names). The Win32
**	set is the correct one here: this tree is built with -DWIN32, and the
**	shipping game was a Win32 Watcom build, so these reproduce the layout the
**	original binary actually had.
**
**	The larger values are also the safe direction -- _splitpath() in wwcompat.cpp
**	writes the full component into the caller's buffer, which a 9-byte _MAX_FNAME
**	would overflow on any modern path.
*/
#ifndef _MAX_PATH
#define _MAX_PATH  260
#endif
#ifndef _MAX_DRIVE
#define _MAX_DRIVE 3
#endif
#ifndef _MAX_DIR
#define _MAX_DIR   256
#endif
#ifndef _MAX_FNAME
#define _MAX_FNAME 256
#endif
#ifndef _MAX_EXT
#define _MAX_EXT   256
#endif

/*
**	File-open modes used by the library's FileClass (WIN32LIB/INCLUDE/WWFILE.H
**	defines READ as _READ). Watcom supplied these; nothing in the tree does. The
**	values must equal the game's own READ/WRITE in CODE/WWFILE.H (1 and 2),
**	because both file hierarchies open the same files with the same flags.
*/
#ifndef _READ
#define _READ  1
#endif
#ifndef _WRITE
#define _WRITE 2
#endif

/*
**	DOS path decomposition. Drive letters never appear on macOS, so the drive
**	component always comes back empty and the directory carries the full path.
*/
void _splitpath(const char * path, char * drive, char * dir, char * fname, char * ext);
void _makepath(char * path, const char * drive, const char * dir, const char * fname, const char * ext);

long filelength(int handle);

/*
**	DOS directory enumeration (_dos_findfirst / _dos_findnext).
**
**	Used on the load path: INIT.CPP registers every "SC*.MIX" scenario archive it
**	finds, and LOADDLG.CPP lists "SAVEGAME.*" and orders the slots by the packed
**	DOS date/time. Implemented for real in wwcompat.cpp on opendir + fnmatch.
**
**	The layout is Watcom's. `size` is uint32_t because Watcom's unsigned long
**	was 32 bits. `name` is the 8.3 field: names that cannot fit are skipped
**	rather than truncated -- DOS could never have returned them, and truncation
**	could make two different files look the same.
*/
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_VOLID  0x08
#define _A_SUBDIR 0x10
#define _A_ARCH   0x20

struct find_t {
	char           reserved[21];	/* port: holds the search-slot index */
	char           attrib;
	unsigned short wr_time;			/* DOS packed: hhhhh mmmmmm sssss (seconds/2) */
	unsigned short wr_date;			/* DOS packed: yyyyyyy mmmm ddddd (year-1980) */
	uint32_t       size;
	char           name[13];
};

unsigned _dos_findfirst(const char * pattern, unsigned attributes, struct find_t * result);
unsigned _dos_findnext(struct find_t * result);
unsigned _dos_findclose(struct find_t * result);

#ifdef __cplusplus
}
#endif

#endif /* WWPORT_COMPAT_WWCOMPAT_H */
