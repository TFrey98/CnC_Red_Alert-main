#!/bin/zsh
#
# flags.sh -- the compiler configuration for the native Apple Silicon build.
#
# Every flag here was derived empirically by compiling the tree and eliminating
# the largest error class at each step. The comments record WHY each one is
# needed, because none of them are obvious and removing any one of them
# re-breaks 180+ translation units.

RA_ROOT="${RA_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"

RA_TARGET=(
  # arm64 explicitly. This build must never depend on Rosetta, which in any
  # case only ever translated x86_64 -- never the 32-bit x86 this code was
  # originally written for.
  -target arm64-apple-macos13
)

RA_STD=(
  -std=c++11

  # CODE/movie.h declares the MPEG player's entry points with
  # __declspec(dllimport). clang accepts __declspec under this flag alone,
  # which is preferable to -fms-extensions: it enables exactly the one
  # construct needed, without the rest of the MSVC dialect.
  -fdeclspec

  # Brace-initialiser narrowing. Watcom compiled C++98, where `char t[] = {200}`
  # is legal and stores the value modulo 256; C++11 made it an error. 581 sites,
  # 508 of them in COORD.CPP's lookup tables, put constants 128-255 into char /
  # signed char. Watcom used signed char (/j), as does Apple arm64, so 200 was
  # stored as 0xC8 (-56) -- exactly the conversion clang performs with this
  # check off. Identical bits, so this restores the original rather than masking
  # a difference.
  -Wno-c++11-narrowing

  # Watcom used one-phase template lookup, so the engine refers to inherited
  # members of dependent base classes without `this->` (see CODE/ftimer.h).
  # This restores that behaviour instead of editing every template.
  -fdelayed-template-parsing
)

RA_DEFINES=(
  # Suppresses the 1994-era `enum {false,true}; typedef int bool;` polyfill in
  # 22 headers, so the real C++ bool is used consistently.
  -DTRUE_FALSE_DEFINED

  # Selects the Win32 code paths over the DOS ones. The Win32 paths are much
  # closer to what a native Cocoa/Metal backend needs.
  -DWIN32

  # Build language. CODE/MAKEFILE defaults LANGUAGE=ENGLISH, and CODE/LANGUAGE.H
  # defines every TEXT_* UI string only inside #ifdef ENGLISH / GERMAN / FRENCH,
  # so without one of these no dialog in the game has any text. Some sites test
  # it by value -- `#if (ENGLISH ...)` -- which -D's implicit value of 1 covers.
  -DENGLISH
)

RA_INCLUDES=(
  # Force-included into every TU: the non-standard Watcom CRT entry points.
  -include "${RA_ROOT}/port/compat/wwcompat.h"

  # CRITICAL: -iquote vs -I is load-bearing, not stylistic.
  #
  # CODE/ and WIN32LIB/INCLUDE/ both contain files named AUDIO.H, DEFINES.H,
  # EXTERNS.H, FILEPCX.H, FUNCTION.H, KEYBOARD.H, MOUSE.H, RAWFILE.H and
  # WWFILE.H -- nine collisions of entirely different content. The original
  # build told them apart by bracket style: <mouse.h> meant the library's
  # WWMouseClass, "mouse.h" meant the game's MouseClass.
  #
  # -iquote applies to "..." only, -I to <...>, which reproduces that exactly.
  # Collapsing these into plain -I silently compiles the wrong headers.
  -iquote "${RA_ROOT}/CODE"

  -I"${RA_ROOT}/port/compat"

  # The plain-C boundary to the native backend (ra_platform.h). Safe on the
  # engine side precisely because it contains no Win32 or Cocoa types.
  -I"${RA_ROOT}/port/backend"
  -I"${RA_ROOT}/WIN32LIB/INCLUDE"
  -I"${RA_ROOT}/WINVQ/INCLUDE"
)

RA_CXXFLAGS=($RA_TARGET $RA_STD $RA_DEFINES $RA_INCLUDES)

# ---------------------------------------------------------------------------
# Library flags. NOT $RA_CXXFLAGS: the libraries were compiled against their own
# include directory only (WIN32LIB\INCLUDE or WINVQ\INCLUDE in their makefiles),
# never the game's. Several header names exist in both CODE/ and
# WIN32LIB/INCLUDE/ with different contents (KEYBOARD.H, MOUSE.H, AUDIO.H, ...);
# with -iquote CODE a library file's #include "keyboard.h" would silently get
# the game's header. Quoted includes here resolve to the file's own directory,
# then the library's include directory.
# ---------------------------------------------------------------------------
# No -DWIN32 here, deliberately. The library makefiles never defined it:
# WIN32LIB/INCLUDE/WWSTD.H does `#ifndef WIN32 / #define WIN32 1 / #include
# <windows.h>`, so each library file gets windows.h from WWSTD.H. Passing -DWIN32
# makes WWSTD.H skip that include and leaves UINT, WORD, LONG etc. undeclared.
# TRUE_FALSE_DEFINED stays: game and libraries must agree on what `bool` is.
RA_LIB_DEFINES=(-DTRUE_FALSE_DEFINED -DENGLISH)
RA_LIB_COMMON=(
  $RA_TARGET $RA_STD $RA_LIB_DEFINES
  -include "${RA_ROOT}/port/compat/wwcompat.h"
  -I"${RA_ROOT}/port/compat"
  -I"${RA_ROOT}/port/backend"
)
RA_WIN32LIB_CXXFLAGS=($RA_LIB_COMMON -I"${RA_ROOT}/WIN32LIB/INCLUDE")
RA_WINVQ_CXXFLAGS=($RA_LIB_COMMON -I"${RA_ROOT}/WINVQ/INCLUDE")

# ---------------------------------------------------------------------------
# Backend (Objective-C++) flags.
#
# DELIBERATELY NOT $RA_CXXFLAGS. The backend must be built WITHOUT -DWIN32 and
# WITHOUT -include wwcompat.h, because the Win32 shim and the Cocoa/Metal
# headers cannot coexist in one translation unit:
#
#   * port/compat/windows.h has `typedef int BOOL`; <objc/objc.h> has
#     `typedef bool BOOL`. A hard typedef redefinition error, unfixable by
#     include ordering.
#   * windows.h defines min/max as function-like macros, and Metal's own
#     MTLAccelerationStructureTypes.h calls `min(a, b, c)` with three args.
#
# The two sides meet only at port/backend/ra_platform.h, which is plain C.
# Keep it that way; see the comment at the top of that file.
# ---------------------------------------------------------------------------
RA_OBJCXXFLAGS=(
  $RA_TARGET
  -fobjc-arc
  -fmodules
  -I"${RA_ROOT}/port/backend"
)

# The whole native stack ships with macOS -- there is nothing to install, and
# nothing to bundle into the .app beyond the binary itself.
RA_FRAMEWORKS=(
  -framework Cocoa
  -framework Metal
  -framework QuartzCore
)
