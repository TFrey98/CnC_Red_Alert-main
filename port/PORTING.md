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

**156 of 277 game translation units in `CODE/` compile cleanly for arm64.**

Run `port/probe.sh` to reproduce that number and see the current blocker list.
It compiles every TU in parallel and buckets the first error from each, so the
largest remaining obstacle is always at the top.

The character of the work has now changed, and this is the single most useful
thing to know before continuing. Every previous session was a hunt for the one
header defect blocking ~185 files at once. **Those are gone.** `probe.sh` now
reports a flat list of 1- and 2-file entries with no shared-header wall behind
them. Expect the count to climb steadily from here rather than in jumps, and do
not go looking for another single high-leverage fix -- there isn't one left.

Of the 121 still failing, about 22 are multiplayer, Westwood Online or serial
files (`WOL_*`, `WSP*`, `MP*`, `NULL*`, `IPX*`, `TCPIP.CPP`, `CONNECT.CPP`,
`QUEUE.CPP`, `SESSION.CPP`) and are out of scope for the first playable build.
The genuine in-scope remainder is roughly 99 files.

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

1. **Finish the header grind** — ~99 in-scope files. Keep running `probe.sh`
   and clearing the top entry. Unlike previous sessions, expect this to climb
   steadily rather than in jumps: the shared-header walls are gone and what
   remains is genuinely per-file. Budget accordingly; this is now a grind
   measured in files, not in insights.
2. ~~Install the toolchain.~~ **Done — nothing to install.** This machine has
   full Xcode 27.0, the Metal toolchain (`metal`/`metallib`, shipped as a
   MobileAsset cryptex), and the macOS 27.0 SDK with Metal, MetalKit,
   QuartzCore, AppKit, AudioToolbox, CoreAudio, AVFoundation, CoreVideo and
   GameController all present. An earlier note in this file claimed only the
   Command Line Tools were available and treated Homebrew/CMake/SDL2 as
   prerequisites; that was wrong on both counts, and irrelevant now that the
   backend is native.
3. **Rewrite the assembly in C.** The bulk of the work: ~29,000 lines across
   `WIN32LIB/DRAWBUFF/` (blitters, `STAMP.ASM`, `SCALE.ASM`, `FILLQUAD.ASM`),
   `WIN32LIB/SHAPE/` (`DRAWSHP.ASM`, 1,128 lines), `WIN32LIB/IFF/` (the LCW
   codec), `WIN32LIB/AUDIO/` (`SOSCODEC.ASM`), `WIN32LIB/KEYBOARD/WWMOUSE.ASM`,
   and `CODE/2KEYFBUF.ASM` (4,848 lines).

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
