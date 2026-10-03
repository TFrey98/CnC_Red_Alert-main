# Porting Red Alert to native Apple Silicon

Working notes for this port. Audience: whoever continues the work — assumes
C/C++ and build-system familiarity, but no prior knowledge of this codebase.

> **Per-file worklist:** `port/WORKLIST.md` tags every file with the work it
> needs (DONE / TWEAK / TRANSLATE / NATIVE / REBUILD / DROP), regenerated from a
> live `probe.sh` run. `port/worklist.json` is the machine-readable twin.
>
> **Archive:** `archive/` holds 1,032 files the port does not build, moved (not
> deleted) with original paths preserved. `archive/README.md` explains what and
> why; `archive/manifest.json` makes it restorable.

> **Companion document:** `port/INVENTORY.md` is the triage — every part of the
> tree classified as tweak / translate / native / rebuild / drop, with measured
> line counts. Read it first if you are deciding scope or estimating effort.
> `port/asm-inventory.json` holds the full machine-readable assembly listing.

## Goal and hard constraint

A genuine `arm64-apple-macos` binary. No Rosetta, no Wine, no x86 of any kind.

This is not just a preference. Rosetta 2 only ever translated **x86_64**, and
this codebase's ~29,000 lines of assembly are **32-bit x86 (i386)**, which
Rosetta never supported. There was never an emulation shortcut available, so
Rosetta's retirement changes nothing about the required approach: every line of
that assembly has to be replaced with portable C. The result is inherently
future-proof, because it depends on no translation layer at all.

Scope for the first playable build: **single-player campaign only.** IPX,
modem/serial and Westwood Online are out. That is also what makes the missing
proprietary SDKs (Greenleaf GCL, the DirectPlay lobby headers) a non-issue —
every file that needs them is in the multiplayer path.

## Current state

**Every in-scope game translation unit compiles for arm64.** Of the 247 in the
build set: **220 compile clean**, **5 are tagged NATIVE** -- the game's Win32
platform layer (`WINSTUB`, `STARTUP`, `KEY`, `CDFILE`, and the DirectDraw/CD
remnants of `CONQUER`), which is reimplemented over `port/backend/` rather than
patched -- and **22 are DROP** (multiplayer and Westwood Online). There is no
TWEAK work left in `CODE/`.

That changes what "next" means. The C++ compile grind for the game is done; the
remaining phases are the platform layer, the libraries (`WIN32LIB/`, `WINVQ/`,
not yet measured by `probe.sh`), the assembly, and then linking -- which will be
the next great source of truth, the way compiling was.

The file layer is already real: `RawFileClass`, through which every MIX, INI and
save file is opened, runs end to end on macOS over a POSIX implementation of the
Win32 file API, and is covered by `port/tests/run.sh`.

Tools, in the order to run them:

| Command | What it tells you |
|---|---|
| `port/probe.sh` | clean / total for the build set, and the biggest blockers |
| `port/worklist.py` | regenerates `WORKLIST.md`: every file tagged, assembly classified by liveness |
| `port/tests/run.sh` | **the data-path tests** -- CRC, SHA-1, RSA against independent references |
| `port/build-backend.sh` | the Metal backend builds, and the engine/Cocoa boundary holds |

**The compile count is not the measure that matters most.** Read the next
section before trusting any number in this file.

## Silent data corruption -- the class of bug that compiles cleanly

The most damaging defects in this port produce no compiler error at all. Each
one below would have let the game compile, link and launch, then fail to read a
single asset, with nothing pointing at the cause. All are fixed and verified.

The root causes are two facts about the original build that `flags.sh` did not
reproduce:

- **`long` was 32 bits on Win32 and is 64 bits on macOS** (LP64). The engine uses
  `long` to mean "32-bit integer" everywhere, including in file formats and
  algorithms.
- **Watcom compiled with `/zp1` -- one-byte struct packing.** (Found in
  `CODE/MAKEFILE`'s `CC_CFG`. The same block shows `/j`, signed `char`, which
  Apple arm64 happens to match.)

What they broke, and how each fix is proven:

| What | Effect before the fix | Proof it is fixed |
|---|---|---|
| MIX archive header and index (`MIXFILE.H`) | 16- and 24-byte records instead of 6 and 12: every archive misread | `static_assert` on the shipped sizes |
| MIX index binary search (`compfunc`) | compared 8 bytes of a 4-byte CRC | same |
| 8 asset-format headers -- `.SHP`, `.AUD`, `.WSA`, `.VQA`, icon sets, keyframes, VQ mix | padding and `long` fields misparse every file | `static_assert` on each original size, evaluated through the real include path |
| `CRCEngine` (`CRC.H/.CPP`) -- hashes MIX filenames | silently dropped bytes 5-8 of each tail: **no file could be found** | 311/311 vs a line-by-line model of `CRC.ASM`; 2000/2000 chunked |
| SHA-1 (`SHA.H/.CPP`) -- MIX digests | wrong hash; `Result()` overflowed a 40-byte digest into 20-byte buffers | FIPS 180 vectors incl. 1,000,000 x 'a' in chunks, ASan-clean |
| RSA (`MP.H`: `#define digit unsigned long`) -- decrypts MIX headers | **0/60** modular exponentiations correct | **56/56** vs Python `pow()` for every valid input |
| WSA animation offsets, icon-set IFF ID, VQA frame table | 8-byte reads of 4-byte fields; `WSA_FILE_HEADER_SIZE` computed as 6, not 14 | the `sizeof(long)` sweep below |

Two details worth knowing before changing any of this:

- **The shipped game used the C `CRCEngine`, not `CRC.ASM`.** `INIT.CPP` defines
  `Calculate_CRC` in C and object files are linked before libraries, so the
  library's assembly version was never pulled in. The fix had to make the C
  version match the assembly the MIX files were built against -- and the test
  checks exactly that.
- **`MP.H` declares `UNITSIZE 32` itself.** The multi-precision library always
  assumed 32-bit digits; the `unsigned long` macro contradicted the library's own
  definition, not just Win32's.

Remaining known instances are listed under "What remains". The sweep that found
the algorithmic ones was simply: every `sizeof(long)` in live code is a place
that reasons about `long`'s width, and on macOS that reasoning is wrong.

### Second pass: more of the same class, found the same way

| What | Effect before the fix | How it was found / proven |
|---|---|---|
| `COORDINATE` / `TARGET` typedef'd to `long` (`DEFINES.H`) | `XY_Coord()` returned `0x0000000156781234` at -O0 and `0x56781234` at -O2: the upper half of every coordinate was stack garbage, so identical coordinates compared unequal depending on build | ran it; `port/tests/coord_compose.cpp`; `static_assert`s on both unions |
| `VesselClass::Take_Damage(..., int forced)` vs base `bool forced` | **not an override any more** -- ships hit through an `ObjectClass*` skipped their own damage logic | `-Woverloaded-virtual` across the whole build set; confirmed it is the only one |
| Keyframe slot cache: `memset(..., frames*4)` on an array of `char *` | only half the cache zeroed; later frames read stale pointers as "already decoded" -- corrupt or crashing unit graphics | reading `2KEYFRAM.CPP`'s pointer-offset code |
| `ShapeHeaderType::shape_data` -- an offset stored in a `char *` | struct grew from 12 to 24 bytes while `2KEYFBUF.ASM` reads it as three dwords | `static_assert(sizeof == 12)` |
| `RawFileClass` passing `&(unsigned long&)bytesread` to `ReadFile` | 4-byte count written into an 8-byte `long`: byte counts half garbage | compiling against the real file API |
| Save-game pointer encoding (`Code_Pointers` / `Decode_Pointers`) | 32-bit ids round-tripped through truncating casts | 12 sites, `(TARGET)(intptr_t)` |
| `CloseHandle` was a no-op stub | once files were real, every close would have leaked a descriptor | `port/tests/win32_file.cpp` checks the descriptor is gone |

Also from this pass, each a Watcom-vs-standard difference with an exact
equivalent rather than a workaround:

- **Friend injection.** Watcom made friend functions defined inside a class
  visible like free functions; standard C++ finds them only through an argument
  of the class type. `FIXED.H` now redeclares its eight named friends at
  namespace scope. (I first misdiagnosed `Sub_Saturate` as missing from the
  release and wrote a duplicate -- it was defined as a friend all along; the
  duplicate is gone.)
- **Temporaries bound to non-const references** -- `ini.Load(CCFileClass("RULES.INI"))`.
  `ww_lvalue()` in `wwcompat.h` gives the temporary exactly its original
  full-expression lifetime; naming it instead would keep files open longer.
- **`TBLACK` as a null pointer** -- an enumerator equal to 0 was accepted as a
  null pointer; the receiving functions explicitly handle `fore == NULL`. 33
  sites now pass `NULL`.
- **`static` members defined `const`, `virtual`/`static` on out-of-line
  definitions, explicit specialisations without `template<>`, implicit `int`.**

Known and deliberately left as-is:

- ~~`RawFileClass::Read` retries a failed read forever.~~ **Resolved:** a
  retryable read error now shows a native **Try Again / Cancel** dialog
  (`RA_Platform_Disk_Error` in `port/backend/ra_dialog.mm`). Try Again retries the
  read; Cancel quits -- `exit()` from `RawFileClass`, `Emergency_Exit()` from the
  game's `CCFileClass`. This restores the contract documented in
  `CODE/CCFILE.CPP` ("pressing a key will return ... otherwise it will exit").
  Non-retryable errors, such as a missing file at open time, stay silent, because
  `Is_Available()` depends on that. Tested with scripted answers
  (`port/tests/disk_error.cpp`); `port/tests/show-disk-error-dialog.sh` shows the
  real dialog for a manual check.
- **The build is a variant that never shipped.** `CODE/MAKEFILE` defined
  `WOLAPI_INTEGRATION` and `WINSOCK_IPX`; this port does not, to keep OLE/COM and
  defunct online code out. 51 files test those symbols. `GAME_VERSION`, which
  lived only in the WOL header, is now supplied in `VERSION.CPP` with the same
  value.
- Two debug `printf`s in `EVENT.CPP` pass `long` to `%d`; harmless on this ABI and
  only printed with `Debug_Print_Events`.

### Related: bool, narrowing, and for-scope

- **Watcom 10.6 had no native `bool`; the engine's polyfill was
  `typedef int bool`.** So in the shipped game `bool` was a 4-byte int. Only one
  variable relied on that -- `ScenarioInit`, a nesting counter (41 `++`, 55 `--`)
  that a real `bool` would collapse. Confirmed exhaustively with clang's
  `-Wdeprecated-increment-bool`, which flags every `++` on a `bool`: it is the
  only one. It is now an `int`.
- **581 brace-initialisers narrow constants 128-255 into `char`** (508 in
  `COORD.CPP`'s tables). C++98 wrapped them; C++11 rejects them.
  `-Wno-c++11-narrowing` restores the wrap, and it was verified on the target
  that `{200}` stores as -56 -- the same bits Watcom produced.
- **74 pre-standard `for`-scope leaks** fixed in total. Each was checked for reads
  of the variable after its loop; exactly one exists (`SCORE.CPP`'s hall-of-fame
  slot), and that one is declared before its loop instead, reproducing Watcom's
  scoping. Note that `index` collides with POSIX `index()` from `<strings.h>`,
  so those leaks report as `non-object type ... is not assignable` rather than
  "undeclared identifier".

## The endianness bug — read this before touching CODE/DEFINES.H

The most consequential defect found so far, because it was silent. It is fixed,
but the shape of it is worth understanding, since the same trap is waiting in
any other BSD-derived build of this tree.

The engine selects the field order of the unions that overlay `COORDINATE`,
`CELL`, `TARGET`, `LEPTON` and the fixed-point type with:

```c
#ifdef BIG_ENDIAN
```

On macOS that test is **always true**. `<machine/endian.h>` defines
`BIG_ENDIAN` unconditionally as the constant `4321` — it is the *name of an
order*, meant to be compared against `BYTE_ORDER`, not an assertion about the
current machine. macOS pulls that header in transitively, so a plain `#ifdef`
sees it defined and picks the big-endian layout on a little-endian arm64 target.

That reverses the byte and bitfield order of the engine's most fundamental
types. It would not have failed to build; it would have produced a game whose
coordinates, cells and targets were quietly scrambled, which is a far worse
outcome than a compile error and would have been extremely hard to trace back
from the symptoms.

There is corroborating evidence the branch had never once been compiled: it
refers to `SLUF_BITS`, while the macro directly above it is spelled
`SLUFF_BITS`.

The fix is `RA_BIG_ENDIAN` in `port/compat/wwcompat.h`, which tests
`__BIG_ENDIAN__` / `__BYTE_ORDER__` properly. All seven `#ifdef BIG_ENDIAN`
sites in `CODE/` now use it. It is deliberately *not* named `BIG_ENDIAN`, so it
cannot collide with the system constant and cannot regress the same way.

Two consequences worth carrying forward:

- **Do not reintroduce a bare `#ifdef BIG_ENDIAN` anywhere.** Use
  `#if RA_BIG_ENDIAN`.
- The big-endian branches are now genuinely dead code on this target, and they
  are **not known to be correct** — the `SLUF_BITS` typo proves at least one was
  never compiled. `CODE/DEFINES.H:540`'s `#if SLUFF_BITS` also expands to
  `sizeof(CELL)*CHAR_BIT-14`, and `sizeof` is not available to the preprocessor,
  so that branch cannot compile as written. Anyone reviving a big-endian target
  is starting from scratch there, not from working code.

## The libraries (`WIN32LIB/`, `WINVQ/`)

**62 of 77 library translation units compile for arm64** -- `port/probe-libs.sh`,
over `port/lib-build-set.txt` (generated from each library's makefile by
`port/gen-lib-set.py`). 9 are DROP (modem, profiler, DOS VESA video, CD
enumeration, and two files superseded outright by the game's own code), 1 is
NATIVE (DirectShow MPEG movies -> AVFoundation). The 5 remaining are the two
batches that belong with the native backend: **audio** (`SOUNDIO`, `SOUNDINT`,
VQA `AUDIO` -- see below) and **DirectDraw** (`GBUFFER`, `DDRAW`).

Things that are different about the libraries, each found by measurement:

- **They are built with their own flags, not the game's.** Each library was
  compiled against its own include directory only; `-iquote CODE` would make a
  library's `#include "keyboard.h"` silently resolve to the game's header. And
  they are built **without `-DWIN32`**: `WWSTD.H` defines `WIN32` and includes
  `windows.h` itself only when `WIN32` is *not* already defined, which is how
  the original library makefiles worked.
- **Duplicate headers.** 72 headers exist both in a module directory and in the
  library's `INCLUDE/`. Module sources compile against their own copy, the game
  against `INCLUDE/` -- so if the two drift, library and game disagree on class
  layouts. Five had drifted (including one of my own earlier fixes, applied to
  one copy only). All are identical now, and `port/check-dup-headers.py`, run by
  `probe-libs.sh`, fails if any drift again.
- **`#ifdef __WATCOMC__`.** 25 files branch on the compiler, and the port took
  the side the shipped game never did. 16 of them guard `#pragma pack` -- so the
  VQA movie format was unpacked. A global `-D__WATCOMC__` would be wrong (it
  pulls in DOS interrupt code), so each class is handled: file-format regions
  get scoped `pack(push,1)`/`pack(pop)` (a literal translation of Watcom's
  `pack()` would have left 1-byte packing on for every later header, system
  headers included); `pack(4)` regions are in-memory and left natural; the DOS
  branches are dropped.
- **The VQA movie format had the 64-bit bug too.** `ChunkHeader` was 16 bytes,
  not 8 -- and it is read with a literal `8`, so both fields landed in `id` and
  `size` was never written. `FormHeader` (the IFF `FORM` that opens every VQA),
  `MIXSubBlock`, `VQHeader`, the WAV headers and the SOS compression header were
  fixed the same way, each with a `static_assert`. `WWTYPES.H`'s `LONG`/`ULONG`
  macros are now 32-bit for the whole player.
- **The library file layer is never used.** `WIN32LIB/RAWFILE/RAWFILE.CPP`
  (built on `mmio*`) is wholly superseded by the game's `CODE/CCFILE.CPP`, which
  defines every symbol the game calls. Dropped; no `mmio` port needed.

**Multimedia timers are real** (`timeSetEvent` & co., in `wwcompat.cpp`). They
drive the game clock (`TIMERINI.CPP`), the mouse, sound maintenance and the VQA
player. All callbacks run on one serial high-priority GCD queue, so -- as on
Win32's single timer thread -- they never overlap each other; `timeKillEvent`
waits for an in-flight callback. Tested under ThreadSanitizer
(`port/tests/mm_timer.cpp`).

**The audio batch needs reading, not casting.** `SOUNDIO`/`SOUNDINT` keep the
DirectSound write position (`StreamType::DestPtr`) as an *offset* stored in a
`void *`, which is truncation-safe -- but the same expressions also mix in real
buffer pointers (`(unsigned)play_buffer_ptr + (unsigned)st->DestPtr` at
`SOUNDIO.CPP:1846`). A mechanical fix of every reported cast would leave that
real pointer truncated: a crash on the first sound.

**Expected at link time.** The original linker resolved duplicate definitions by
order; a modern one will report them. Known now: `LOAD.CPP` (live library code)
and `CODE/CCFILE.CPP` both define `Load_Data` / `Load_Alloc_Data`, and
`Stop_Profiler` needs a stub because the profiler is dropped.

## A note on searching this tree

`grep` in some shells here is a wrapper around `ugrep` with `-I` (skip binary
files) baked in. Several files in `CODE/` trip its binary heuristic — `FUNCTION.H`
among them — and are **silently skipped**, with no match and no warning. A
search for `phone.h` in `FUNCTION.H` returns nothing while the file plainly
contains it.

This matters for a port that proceeds by auditing every occurrence of a
construct. Use `command grep -rn` (or `grep -a`) for anything you intend to
treat as exhaustive. The `BIG_ENDIAN` sweep above was re-run that way to confirm
all seven sites were found.

## What has been done

### `port/compat/` — the platform shim

Sixteen headers plus implementations, standing in for the Win32 SDK and the
DOS-era CRT. The engine still thinks it is calling Windows; nothing above this
layer knows otherwise.

- `windows.h` — Win32 types, handles, geometry, threading. **The single
  highest-leverage file in the port:** it is included by `CODE/function.h`,
  which nearly every game file includes, so it alone unblocked ~193 TUs.
- `ddraw.h` / `dsound.h` / `mmsystem.h` — DirectDraw and DirectSound as C++
  abstract classes matching the original virtual-call style. Declarations only;
  the native implementations behind them are the next major piece of work.
  `ddraw.h` is now backed by `port/backend/` (Metal); `dsound.h` is not yet.
- `wwcompat.h` / `wwcompat.cpp` — the non-standard Watcom CRT: `itoa`, `ltoa`,
  `strupr`, `stricmp`, `_lrotl`, `_splitpath`, `filelength` and friends.
  Force-included via `-include` so the 300-odd original sources stay untouched.
- `io.h`, `dos.h`, `mem.h`, `malloc.h`, `new.h`, `direct.h`, `conio.h`,
  `bios.h`, `share.h`, `process.h`, `objbase.h`, `winsock.h`, `windowsx.h`,
  `iostream.h` — small mappings onto POSIX equivalents.

- **Stubs for the missing proprietary SDKs** — `modem.h`, `fast.h` (Greenleaf
  GCL), `commlib.h` (GCL serial), `phone.h` (modem phone-list manager). All four
  are included unconditionally by `CODE/function.h`, so they must exist for every
  TU even though single-player touches none of them.

  They are deliberately **empty**, not filled with plausible prototypes. A
  missing declaration fails loudly at the exact line that needs it, which marks
  the modem path precisely; fake prototypes would compile and then fail at link
  time with nothing pointing at the cause. Each stub's header comment records
  why empty is provably sufficient — e.g. `SESSION.H` forward-declares
  `PhoneEntryClass` itself and only stores pointers to it.

  The one exception is `commlib.h`, which declares `PORT` as an **opaque** type.
  Four sites outside `#ifdef WIN32` name it, but all four use it only as a
  pointer. Leaving it undefined means any attempt to dereference or size it
  fails clearly rather than compiling against an invented layout.

- `pcx.h` — **not a stub.** `CODE/function.h` includes `"pcx.h"`, but the release
  ships that header as `FILEPCX.H`; the giveaway is that `FILEPCX.H`'s own
  include guard is still `PCX_H`. `port/compat/pcx.h` forwards to it.
  `CODE/FILEPCX.H` and `WIN32LIB/INCLUDE/FILEPCX.H` were verified byte-for-byte
  identical, so the usual `-iquote`/`-I` split does not matter here.

- Additions to `windows.h` this session: `min`/`max` macros (see below),
  `OVERLAPPED` (laid out, because `WIN32LIB/INCLUDE/wincomm.h` embeds two by
  value), the DDE handles `HSZ`/`HDDEDATA`/`HCONV`, `SOCKET`, and the
  pointer-width `ULONG_PTR`/`LONG_PTR`/`DWORD_PTR`.

- Additions to `winsock.h`: the uppercase Winsock aliases (`IN_ADDR`,
  `SOCKADDR_IN`, `HOSTENT`), `WSADATA` and `MAXGETHOSTSTRUCT`.

- `min`/`max` in `windows.h` are **macros, and must stay macros.** Real
  `<windows.h>` defines them that way unless `NOMINMAX` is set, and ~37 files
  depend on macro behaviour: call sites mix argument types, as in
  `min(scatterdist, Rule.HomingScatter)` in `BULLET.CPP`, which pairs an `int`
  with a fixed-point member. A `template<class T> T min(T,T)` cannot deduce `T`
  there. They carry Win32's double-evaluation hazard, which is kept deliberately
  so the port does not quietly change evaluation counts. The engine's own
  capitalised `Min`/`Max` templates in `WIN32LIB/INCLUDE/WWSTD.H` are separate
  and unaffected. Only `<new>` is included tree-wide, so there is no
  `<algorithm>` for these macros to break — re-check that before adding any C++
  standard header.

### Source changes

Kept deliberately minimal, to hold the diff against EA's release small and
reviewable. So far:

- Stripped trailing DOS EOF bytes (`0x1A`) from **146 files**. Clang treats one
  as a stray token; every occurrence was verified to be in the trailing bytes
  with no content after it before anything was written.
- Converted backslash `#include` paths to forward slashes in **80 files**
  (`<vqa32\vqaplay.h>` → `<vqa32/vqaplay.h>`). Only the path inside an
  `#include` was touched, never a string literal or line continuation.
- `WIN32LIB/INCLUDE/GBUFFER.H:372` — removed a redundant `GraphicBufferClass::`
  qualification on a member declared inside its own class.
- `CODE/VECTOR.H:68` — replaced hand-rolled placement `new` with `<new>`;
  libc++ declares both forms `noexcept` and redeclaring them without it is an
  error.
- `CODE/INT.H:184` — dropped a default argument from a non-defining friend
  declaration. Both call sites pass it explicitly, so nothing changes.
- `CODE/MIXFILE.H:44` — `Node<MixFileClass>` → `Node<MixFileClass<T> >`; the
  injected-class-name is not in scope in a base-clause.

- **`CODE/DEFINES.H` (4 sites), `CODE/FIXED.H`, `CODE/BASE64.CPP` (2 sites)** —
  `#ifdef BIG_ENDIAN` → `#if RA_BIG_ENDIAN`. See the section above.
- `CODE/CCFILE.H:105` and `CODE/FUNCTION.H` (6 sites) — removed
  `class SomeTemplate<Arg>;` lines. A specialization cannot be forward-declared
  without `template<>`, and adding `template<>` would be *worse* than the error:
  it declares an explicit specialization and leaves the type undefined. All
  seven were redundant anyway — each sat next to a declaration that already
  names the type. The odd neighbouring variables (`y002`, `xxx1`, `whatever`)
  are a Watcom idiom: dummy externs that forced template instantiation.
- `CODE/radar.h:91` — removed a redundant `RadarClass::` qualification on a
  member declared inside its own class (same defect as `GBUFFER.H:372`).
- **`CODE/ftimer.h` — 16 sites**, added `this->` to `Started` and `Timer()` in
  `TTimerClass<T>` and `CDTimerClass<T>`. These are members of the dependent
  base `BasicTimerClass<T>`. Note this contradicts a reasonable reading of the
  `-fdelayed-template-parsing` flag comment: that flag defers *parsing*, but
  clang still refuses to look into dependent bases for unqualified names. Only
  `-fms-compatibility` would do that, and it drags in the rest of the MSVC
  dialect. Adding `this->` is standard C++ and costs nothing. **Expect more of
  these** as further templates get instantiated.
- `CODE/TCPIP.H` — added `#include "winsock.h"`. The file declares `SOCKET` and
  `struct in_addr` members but included nothing, relying on Win32's
  `<windows.h>` pulling in `<winsock.h>`.

### Build configuration

`port/flags.sh` holds the flag set, with the reasoning for each. Two are
non-obvious and worth knowing about before touching them:

- **`-iquote` vs `-I` is load-bearing.** `CODE/` and `WIN32LIB/INCLUDE/` both
  contain `AUDIO.H`, `DEFINES.H`, `EXTERNS.H`, `FILEPCX.H`, `FUNCTION.H`,
  `KEYBOARD.H`, `MOUSE.H`, `RAWFILE.H` and `WWFILE.H` — nine collisions of
  completely different content. The original build distinguished them by
  bracket style. `-iquote` for `"..."` and `-I` for `<...>` reproduces that.
  Collapsing them into plain `-I` compiles the wrong headers *silently*.
- **`-fdelayed-template-parsing`** defers parsing of template bodies. Note the
  limit of this flag, discovered the hard way: it does **not** make clang search
  dependent base classes for unqualified names. Members inherited from a
  template base still need `this->` (see the `ftimer.h` entry below).

- **`-fdeclspec`** — `CODE/movie.h` declares the MPEG player's entry points with
  `__declspec(dllimport)`. This flag enables exactly that one construct, which
  is preferable to `-fms-extensions` pulling in the rest of the MSVC dialect.

## Build shape: scripts, not an .xcodeproj

Native macOS tooling here means the **compilers and frameworks**, not the Xcode
IDE. The build is deliberately plain shell scripts driving `clang++` and
`xcrun`, with no `.xcodeproj` and no `xcodebuild`.

That is a requirement, not an accident: the maintainer works in VS Code because
AI coding agents have repeatedly crashed Xcode on this project. Everything here
must stay runnable from a terminal. Full Xcode is still needed as the *provider*
of the SDK and the Metal toolchain — `xcode-select -p` points at it — but
nothing in this port should require opening it.

Practical consequences if you extend the build:

- Keep using `xcrun -sdk macosx …` to locate tools. It resolves the Metal
  compiler out of its MobileAsset cryptex, which is not on `PATH`.
- If a real build system becomes warranted, prefer a Makefile or CMake with the
  Ninja/Makefile generator over CMake's Xcode generator.
- `codesign` and `notarytool` are both command-line; step 6 needs no IDE either.

## The native backend — and the boundary rule that governs it

`port/backend/` holds the Metal display backend. Build and verify it with
`port/build-backend.sh`, which is fast and should be run after any change to
either the backend or the Win32 shim.

### The rule: the Win32 shim and Cocoa/Metal must never share a translation unit

This is the single most important structural constraint in the port, and it is
not a stylistic preference — the two are genuinely incompatible:

- `port/compat/windows.h` has `typedef int BOOL`. `<objc/objc.h>` has
  `typedef bool BOOL`. That is a hard typedef redefinition error, and **no
  include ordering fixes it.**
- `windows.h` defines `min`/`max` as function-like macros, and Metal's own
  `MTLAccelerationStructureTypes.h` calls `min(a, b, c)` with three arguments.

So the split is enforced structurally, by the build:

| | engine `.CPP` | backend `.mm` |
|---|---|---|
| flags | `$RA_CXXFLAGS` | `$RA_OBJCXXFLAGS` |
| `-DWIN32`, `-include wwcompat.h` | yes | **no** |
| may include Cocoa/Metal | **no** | yes |
| may include engine headers | yes | **no** |

The two sides meet at exactly one file: `port/backend/ra_platform.h`, which is
**plain C only** — no Win32 types, no engine headers, no C++. Keeping it austere
is what makes the rest work. `build-backend.sh` step 3 exists specifically to
catch a regression here.

### What the display backend does

The engine renders to an 8-bit paletted framebuffer and mutates the palette
directly for fades. That model is preserved all the way to the GPU rather than
being flattened on the CPU:

- indices upload as an `r8uint` texture — **not** `r8unorm`, and never sampled
  with filtering. Palette indices are identifiers, not intensities;
  interpolating between index 3 and index 4 yields an unrelated colour, not a
  blend of two palette entries.
- the palette is a 256×1 `rgba8unorm` texture, so a palette change is a 1KB
  upload rather than a re-expansion of the framebuffer.
- a fullscreen triangle (three vertices, no vertex buffer) runs the lookup in
  the fragment shader.

The shader is compiled from source at startup via `newLibraryWithSource:`
rather than loaded from a prebuilt `.metallib`. That costs a few milliseconds
once and removes a build-time Metal toolchain dependency and a resource file
that could drift out of sync with the code. `ra_palette.metal` carries the same
source for offline compilation and editing; `build-backend.sh` compiles it as a
sanity check.

### Verified so far

On this machine, end to end: shader compiled offline *and* at runtime, pipeline
state created, textures created, window created, palette and framebuffer
uploaded, drawable acquired, render pass encoded and committed to the GPU.
Engine-side and backend-side objects linked into a native arm64 Mach-O.

**Not yet verified:** that pixels appear on screen. There is no `NSApplication`
run loop yet, and the engine cannot call the backend until it builds. Treat the
display path as structurally proven but not visually confirmed.

### Caveat on the palette

`RA_Display_SetPalette` takes 0–255 per channel. The engine's palettes are
mostly VGA 6-bit (0–63) and must be scaled before they get here. The backend
deliberately does not guess, because the engine also uses genuinely 8-bit
palettes in places, and silently rescaling both would corrupt one of them.

## What remains, roughly in order

1. ~~**Finish the compile grind for `CODE/`.**~~ **Done** -- 0 TWEAK files remain.
   The five NATIVE files are item 4's work.

   **The libraries are not measured yet.** `probe.sh` covers `CODE/` only. The
   ~200 C++ files in `WIN32LIB/` and `WINVQ/` have had their on-disk structs
   fixed but have not been through the grind, and several fail on shared header
   conflicts (e.g. a library header defining `BOOL` before `windows.h` typedefs
   it). Extending `probe.sh` to them is the natural next step after `CODE/`.

   **Finish the `long` audit.** Done for everything that reads asset data. Still
   open: `2KEYFRAM.CPP` (keyframe offsets), and a broader review of `long` in
   fixed-point and overflow-sensitive arithmetic, which the `sizeof(long)` sweep
   would not catch.

2. ~~Install the toolchain.~~ **Done — nothing to install.** This machine has
   full Xcode 27.0, the Metal toolchain (`metal`/`metallib`, shipped as a
   MobileAsset cryptex), and the macOS 27.0 SDK with Metal, MetalKit,
   QuartzCore, AppKit, AudioToolbox, CoreAudio, AVFoundation, CoreVideo and
   GameController all present. An earlier note in this file claimed only the
   Command Line Tools were available and treated Homebrew/CMake/SDL2 as
   prerequisites; that was wrong on both counts, and irrelevant now that the
   backend is native.
3. **Rewrite the assembly in C** -- now measured, see `WORKLIST.md`.
   `port/worklist.py` classifies every file by *liveness*: a file is live if live
   code calls something it exports (as a closure, since assembly calls assembly).
   Result: **62 files / ~24,000 lines to translate**, 8 SUPERSEDED (a C version
   already exists in the tree -- CRC, both facing routines, `LCW_Uncompress`, the
   SOS ADPCM decoder), 2 NATIVE, 3 REBUILD, 2 DEAD.

   Lessons from building that classification, each learned by getting it wrong:
   - **Neither `CODE/MAKEFILE` nor `RA95.PJT` says what the game linked.**
     `UNIT.CPP` calls `Fixed_To_Cardinal`, defined only in `COORDA.ASM`, which
     the makefile never names; the `.PJT` lists both halves of every DOS/Win32
     pair. Liveness has to come from references.
   - **Scan headers, and treat `::Name(` as a global call.** The first automated
     pass marked most of the renderer (`DRAWSHP`, the `DRAWBUFF` blitters) as
     dead, because they are reached through inline wrappers in `GBUFFER.H` that
     call `::Buffer_Fill_Rect(this, ...)`. It was caught because the result
     looked too good.
   - **A name match is not equivalence.** Every SUPERSEDED entry needs the C
     version proven to compute the same thing -- which is how the `CRCEngine`
     defect above was found.
   - Already-translated C turned up three times (`INTERPAL.CPP`, `ADPCM.CPP`,
     `LCWUNCMP.CPP`). Westwood did some of this work themselves and left both
     versions in the tree.

   Original guidance for the translation itself still applies:

   The largest live files are `CODE/2KEYFBUF.ASM` (4,848 lines -- frame-buffer
   blitters despite the name), `WINVQ/VQA32/UNVQBUFF.ASM` (the VQ movie decoder),
   `WIN32LIB/SHAPE/DRAWSHP.ASM` and the `WIN32LIB/DRAWBUFF/` blitters. LCW
   decompression and the SOS ADPCM decoder are **not** on the list -- C versions
   already exist (`OLSOSDEC.ASM`'s `General_` variant should be adapted from
   `ADPCM.CPP`, not translated from scratch).

   These are well-understood algorithms — run-length blits, palette remaps,
   LCW/format80 decompression — and are worth writing as straightforward C and
   letting clang vectorise for NEON, rather than transliterating x86 by hand.
   [Vanilla Conquer](https://github.com/TheAssemblyArmada/Vanilla-Conquer) has
   already done this under a GPLv3-compatible licence and is the obvious
   reference, though it is based on the 2020 Remastered drop rather than this
   1997 tree.
4. **Implement the remaining native backends.** The display backend exists and
   is verified (see "The native backend" below). Still to write: audio
   (`dsound.h` → AudioQueue or AVAudioEngine), input (`NSEvent` for keyboard and
   mouse, `GameController.framework` if gamepads are wanted), and timing
   (`mach_absolute_time` for the engine's tick source, `CVDisplayLink` or
   `CAMetalDisplayLink` to pace presentation).

   **Decision made: native Metal/Cocoa, no SDL2.** The reasons, recorded so this
   is not relitigated:

   - **Nothing to install, nothing to ship.** The whole stack is in the macOS
     SDK. No Homebrew, no `macdylibbundler`, no third-party dylibs inside the
     `.app`, and codesigning/notarisation has only our own binary to consider.
   - **The 8-bit paletted model survives to the GPU.** Indices upload as
     `r8uint` and resolve against a 256×1 palette texture in the fragment
     shader. A palette change — which this engine does constantly for fades — is
     a 1KB upload rather than a re-expansion of the framebuffer.
   - Independence from the other port projects. Vanilla Conquer remains useful
     as an *algorithmic reference* for the assembly rewrite in step 3, which is
     backend-agnostic work, but nothing structural is inherited from it.

5. **Audit 64-bit correctness.** `probe.sh` has already surfaced real instances
   of *"cast from pointer to smaller type loses information"* in `LCW.CPP` and
   `LCWUNCMP.CPP`. These are genuine ILP32 assumptions and each needs reading,
   not a blanket cast.

   Related latent bug: `CODE/JSHELL.H:187`'s `_rotl` template computes
   `X >> (sizeof(T)*8 - n)`, which is undefined behaviour when `n == 0`. It
   happened to work on x86; do not assume it will survive clang's optimiser on
   arm64.
6. **Bundle and sign** — `.app` layout, then codesign and notarize. Much
   simpler than originally scoped: with no third-party dylibs there is nothing
   for `macdylibbundler` to do, and no bundled library to sign separately.

## Things to watch

- **Case-insensitive filesystem.** This tree mixes `FUNCTION.H` and
  `function.h` freely and only builds today because macOS volumes are
  case-insensitive by default. It will not build on a case-sensitive volume,
  and neither will CI on Linux. Worth normalising deliberately at some point.
- **No version control yet.** This directory is not a git repository. Given the
  volume of mechanical edits ahead, initialising one before going further is
  strongly recommended — several changes so far touched 80–146 files at once.
- **Endianness is not a concern.** x86 and arm64 are both little-endian, so the
  on-disk MIX/SHP/AUD formats need no byte swapping.
