# Port inventory — tweak / translate / native / rebuild

Companion to `PORTING.md`. That file records *how* the port works and what has
been done; this one is the triage: for every part of the tree, which of four
things has to happen to it.

Audience: whoever continues this port, and whoever decides how much of it is
worth carrying forward versus rebuilding.

Every number here was measured from the tree, not estimated. Where something is
genuinely undetermined it says so rather than guessing.

## The four categories

| Category | Meaning |
|---|---|
| **Tweak** | Keeps its logic. Needs small, mechanical source edits — a `this->`, a missing include, a malformed declaration — to satisfy clang. The engine's own game code is almost entirely here. |
| **Translate** | The algorithm is sound and worth keeping, but the *form* is unusable: 32-bit x86 assembly. Rewrite as portable C, same behaviour. |
| **Native** | The function is still needed but the implementation is tied to a platform that no longer exists. Replace with a macOS framework behind an existing interface. |
| **Rebuild** | No usable source exists, or the concept does not survive the move. Must be written fresh. |
| **Drop** | Not needed on this target at all. Included because it is a large fraction of the tree, and knowing what *not* to port is as valuable as knowing what to. |

## Headline numbers

| | Files | Lines | Notes |
|---|---|---|---|
| Game code — **tweak** | 277 `.CPP` in `CODE/` | ~314,000 (incl. headers) | 156 already compile clean for arm64 |
| Assembly — **translate** | 71 | 24,151 | the real bulk of the remaining work |
| Assembly — **native** | 5 | 3,701 | |
| Assembly — **rebuild** | 3 | 450 | |
| Assembly — **drop** | 15 | 9,271 | |

**On the assembly count.** The tree contains 357 `.ASM` files totalling 159,990
lines, and that number is badly misleading. Most of it is duplication: `WWFLAT32`
is the DOS-era twin of `WIN32LIB`, `VQ` the DOS twin of `WINVQ`, and both carry
`OLD/`, `NEW/`, `X/`, `SRCDEBUG/` and `TEST/` copies. Restricted to the Win32
build path the real figure is **94 files, 37,573 lines**, of which
**24,151 actually need translating**. `PORTING.md`'s earlier estimate of
"~29,000 lines" was close to right for that bucket and should not have been
alarming.

### How the DOS/Win32 split was established

Not by inference from filenames. `CODE/MAKEFILE` line 401 selects objects on
`!ifdef WIN32`, and the `2`-prefixed files are the Win32 members of each pair:

```
!ifdef WIN32
OBJECTS += 2KEYFBUF.OBJ & ... 2TXTPRNT.OBJ & ... 2KEYFRAM.OBJ ...
!else
OBJECTS += KEYFBUFF.OBJ & TXTPRNT.OBJ & KEYFRAME.OBJ
!endif
```

Because this port builds with `-DWIN32`, `KEYFBUFF.ASM`, `TXTPRNT.ASM` and
`SUPPORT.ASM` are the DOS halves and drop out — 3,804 lines that never need
touching. `2KEYFBUF.ASM` is the one that matters, and despite the name it is not
keyboard code: its exports are frame-buffer blitters (`Buffer_Frame_To_Page`).

---

## Translate — 71 files, 24,151 lines

Well-understood algorithms: run-length and masked blits, palette remaps, LCW
(format80) and VQ decompression, line drawing, text rasterising. Write these as
straightforward C and let clang vectorise for NEON. Transliterating x86 by hand
is the wrong approach — it preserves register-allocation decisions that stopped
being relevant in 1997 and makes the result harder to verify.

[Vanilla Conquer](https://github.com/TheAssemblyArmada/Vanilla-Conquer) has C
versions of most of these under a GPLv3-compatible licence. It is built on the
2020 Remastered tree rather than this 1997 one, so it is an *algorithmic
reference*, not a source of drop-in code — but for confirming what a routine is
supposed to do it is the best available.

**None of these have a C counterpart already in the tree.** That was checked
directly: no in-scope `.ASM` file has a same-named `.CPP` sibling.

| File | Lines |
|---|---|
| `CODE/2KEYFBUF.ASM` | 4,848 |
| `WINVQ/VQA32/UNVQBUFF.ASM` | 1,153 |
| `WIN32LIB/SHAPE/DRAWSHP.ASM` | 1,128 |
| `WIN32LIB/WSA/XORDELTA.ASM` | 669 |
| `WIN32LIB/DRAWBUFF/FILLQUAD.ASM` | 669 |
| `WIN32LIB/DRAWBUFF/STAMP.ASM` | 600 |
| `WIN32LIB/DRAWBUFF/SCALE.ASM` | 570 |
| `CODE/2SUPPORT.ASM` | 564 |
| `CODE/2TXTPRNT.ASM` | 507 |
| `WIN32LIB/DRAWBUFF/TXTPRNT.ASM` | 502 |
| `WIN32LIB/DRAWBUFF/DRAWLINE.ASM` | 464 |
| `WIN32LIB/DRAWBUFF/BITBLIT.ASM` | 462 |
| `WIN32LIB/FONT/TEXTPRNT.ASM` | 436 |
| `WIN32LIB/PALETTE/PAL.ASM` | 410 |
| `WINVQ/VQM32/DRAWCHAR.ASM` | 395 |
| `WINVQ/VQM32/HUFFDCMP.ASM` | 391 |
| `WINVQ/VQM32/AUDUNZAP.ASM` | 375 |
| `WIN32LIB/AUDIO/AUDUNCMP.ASM` | 374 |
| `WIN32LIB/SHAPE/DS_DSR.ASM` | 341 |
| `WIN32LIB/SHAPE/DS_DS.ASM` | 341 |
| `WINVQ/VQM32/PALETTE.ASM` | 319 |
| `WIN32LIB/DRAWBUFF/TOPAGE.ASM` | 294 |

…and 49 smaller files. Full list: `port/asm-inventory.json`.

### Duplication still inside this bucket

There are four separate text rasterisers here — `CODE/2TXTPRNT.ASM`,
`WIN32LIB/DRAWBUFF/TXTPRNT.ASM`, `WIN32LIB/FONT/TEXTPRNT.ASM` and
`WINVQ/VQM32/TEXTPRNT.ASM` — and two LCW codecs (`WIN32LIB/IFF/` and
`WINVQ/VQM32/`). Determine which are actually linked before writing four
versions of the same thing. This is unresolved and worth an hour before starting.

---

## Native — 5 files, 3,701 lines, plus the platform layer

### Assembly in this bucket

| File | Lines | Replacement |
|---|---|---|
| `WINVQ/VQM32/SOSCODEC.ASM` | 1,271 | HMI SOS ADPCM decode. Keep the algorithm (translate it), but it feeds CoreAudio, not a Sound Blaster. |
| `WIN32LIB/AUDIO/OLSOSDEC.ASM` | 755 | as above |
| `WIN32LIB/AUDIO/SOSCODEC.ASM` | 724 | as above |
| `WIN32LIB/KEYBOARD/WWMOUSE.ASM` | 662 | Software cursor with shadow-buffer save/restore. Either `NSCursor`, or composite the cursor in the Metal shader — the latter keeps it pixel-exact. |
| `WIN32LIB/PLAYCD/PLAYCD.ASM` | 289 | Redbook CD audio. The music is on disc in the original; AVFoundation plays extracted tracks. |

### The platform layer — the real "native" work

This is interface replacement, not line-by-line porting, and it is already
partly done.

| Subsystem | Original | Native replacement | State |
|---|---|---|---|
| Display | DirectDraw (`ddraw.h`) | **Metal** — `port/backend/ra_metal.mm` | **Working.** Paletted `r8uint` + 256×1 palette texture, lookup in the fragment shader. Verified to encode and commit a render pass on this machine. |
| Audio | DirectSound (`dsound.h`) | AudioQueue or AVAudioEngine | Not started. `dsound.h` shim declares the interface; nothing implements it. |
| Input | Win32 messages, `WWMOUSE.ASM` | `NSEvent`; `GameController` if wanted | Not started. |
| Timing | `timeGetTime`, Win32 timers | `mach_absolute_time`; `CAMetalDisplayLink` to pace frames | Not started. |
| Window / run loop | `WW_WIN` | `NSApplication` + `NSWindow` | Window creation done; no run loop yet. |
| CD / music | Redbook | AVFoundation | Not started. |
| Movies | see below | AVFoundation | Not started. |

**The constraint governing all of it:** the Win32 shim and Cocoa/Metal headers
cannot share a translation unit — `BOOL` is `int` in one and `bool` in the
other, and `windows.h`'s `min`/`max` macros break Metal's own headers. Every
native backend must sit behind a plain-C boundary header like
`port/backend/ra_platform.h`. `PORTING.md` documents this in full; it is not
negotiable and it shapes every remaining item in this table.

---

## Rebuild — no usable source exists

### Assembly (3 files, 450 lines)

| File | Lines | Why rebuild rather than translate |
|---|---|---|
| `CODE/CPUID.ASM` | 185 |
| `WIN32LIB/MISC/OPSYS.ASM` | 150 |
| `WIN32LIB/MISC/DETPROC.ASM` | 115 |

`CPUID.ASM`, `OPSYS.ASM` and `DETPROC.ASM` detect the x86 CPU model, feature
bits and host OS. There is no arm64 translation of that concept. They need new
logic that answers the questions the engine actually asks — and in most cases
the honest answer is a constant, since every target is a known-good arm64 Mac.

### Missing SDKs and libraries

42 headers are referenced and genuinely absent from the tree. They group cleanly:

| Group | Headers | Verdict |
|---|---|---|
| **Movie playback** | `amstream.h`, `ddstream.h`, `mmstream.h`, `streams.h`, `digitalv.h` | **Rebuild on AVFoundation.** Worse than it looks: `CODE/movie.h` declares `MpgPlay` etc. as `__declspec(dllimport)` — the MPEG player was a **separate DLL that was never released**. There is no source to port. The in-game `.VQA` path (`WINVQ/`) *is* present and translatable, so cutscenes are recoverable; the MPEG path is not. |
| **OLE/COM** | `ole2.h`, `oaidl.h`, `ocidl.h`, `olectl.h`, `rpc.h`, `rpcndr.h` | **Drop.** Reachable only from Westwood Online and DirectShow. |
| **Westwood Online** | `ten.h`, `rtq.h`, `services.h`, `lpc.h`, `mplib.h`, `mplpc.h`, `mplayer.h`, `magic.h`, `passedit.h`, `cbn_.h`, `mgenord.h` | **Drop.** The service has not existed for two decades. Note `CODE/WOLAPI/` *does* contain `WOLAPI.H`, `CHATDEFS.H` and friends — the API headers survived; the libraries behind them did not. |
| **IPX / NetWare** | `wsipx.h`, `wsnwlink.h`, `nspapi.h`, `svcguid.h` | **Drop.** |
| **DOS extenders** | `pharlap.h`, `pldos32.h`, `pltypes.h`, `i86.h`, `ems.h`, `vdmdbg.h` | **Drop.** Dead under `-DWIN32`. |
| **Ordinary Win32/CRT** | `commctrl.h`, `winerror.h`, `memory.h`, `stat.h`, `types.h`, `timeb.h`, `algo.h` | **Tweak** — small additions to `port/compat/`, same as the 20 shims already there. |
| **Resource scripts** | `debug.rh`, `strings.rh`, `text.rh` | **Rebuild** if the dialogs are wanted; the `.rh` files defining control IDs are absent. |

### `PaletteClass` — a missing core header, found the hard way

**`CODE/PALETTE.H` and `PALETTE.CPP` are absent from EA's release.** The class
is used by 18 files, including `CONQUER.CPP`, `JSHELL.CPP` and `OPTIONS.CPP`,
and `EXTERNS.H:321` declares `extern PaletteClass GamePalette;` — but the only
trace of the class itself anywhere in the tree (archive included) is a forward
declaration at `CODE/RGB.H:41`.

This did not appear in the missing-header list above, and the reason is worth
understanding. `CODE/function.h:318` has `#include "palette.h"`. That file does
**not** exist in `CODE/`, so the quoted include falls through `-iquote` to `-I`
and silently resolves to `WIN32LIB/INCLUDE/PALETTE.H` — the *library's* C
palette functions, an entirely different header that happens to share the name.
No "file not found" is ever reported. It is the nine-way header collision that
`flags.sh` warns about, in its most dangerous form: not two files fighting, but
a missing one quietly answered by a stranger.

Rebuilding it is tractable. The required surface, taken from actual call sites:

- `PaletteClass::COLOR_COUNT` — static count, 256
- `PaletteClass::CurrentPalette` — static instance
- `operator[](int)` returning `RGBClass &` (`CurrentPalette[1].Red_Component()`)
- `Set(...)` — the fade entry point (`GamePalette.Set(FADE_PALETTE_MEDIUM)`)

`RGBClass` itself survives intact in `RGB.H`/`RGB.CPP`, so this is a container
over an existing, working type rather than a from-scratch reimplementation.

### Already handled

Four proprietary SDKs that blocked every translation unit are now stubbed and no
longer block anything: Greenleaf GCL (`modem.h`, `fast.h`, `commlib.h`) and the
modem phone-list manager (`phone.h`). All are single-player-irrelevant. See
`PORTING.md` for why empty stubs are provably sufficient.

---

## Drop — 15 files, 9,271 lines of assembly

| File | Lines |
|---|---|
| `CODE/KEYFBUFF.ASM` | 2,739 |
| `CODE/WINASM.ASM` | 890 |
| `WINVQ/VQM32/MONO.ASM` | 871 |
| `WINVQ/VQM32/XMODE.ASM` | 748 |
| `WINVQ/VQA32/UNVQXMDE.ASM` | 724 |
| `WINVQ/VQM32/VESABUF.ASM` | 722 |
| `CODE/SUPPORT.ASM` | 559 |
| `CODE/TXTPRNT.ASM` | 506 |
| `WINVQ/VQA32/UNVQVESA.ASM` | 380 |
| `CODE/IPXREAL.ASM` | 319 |
| `WIN32LIB/PROFILE/APROFILE.ASM` | 298 |
| `WINVQ/VQM32/MCGABUF.ASM` | 196 |
| `WINVQ/VQM32/PORTIO.ASM` | 116 |
| `CODE/IPXPROT.ASM` | 113 |
| `WIN32LIB/MEM/VMPAGEIN.ASM` | 90 |

Rationale by group:

- **DOS video hardware** — `XMODE`, `VESABUF`, `MCGABUF`, `UNVQXMDE`,
  `UNVQVESA`, `PORTIO`. Mode X, VESA and MCGA banked framebuffers, driven by
  direct port I/O. A Metal backend presents one paletted surface; none of these
  paths exist.
- **DOS variants superseded by the `2`-prefixed Win32 files** — `KEYFBUFF`,
  `TXTPRNT`, `SUPPORT`, per `CODE/MAKEFILE`.
- **`MONO.ASM`** — debug output to a second monochrome MDA monitor.
- **`IPXREAL`, `IPXPROT`, `WINASM`** — DOS IPX real/protected mode, and Greenleaf
  serial port routines (`WINASM.ASM` exports `PortOpenGreenleafFast_`,
  `FastGetPortHardware_`). Multiplayer and modem: out of scope.
- **`APROFILE.ASM`** — x86 cycle-counter profiler. Instruments does this now.
- **`VMPAGEIN.ASM`** — DOS virtual-memory paging helper.

### Whole directories out of scope

`WWFLAT32/` (100,890 lines) and `VQ/` (37,758) are the DOS twins of `WIN32LIB/`
and `WINVQ/`. `IPX/` (8,270) is 16-bit DOS IPX. `LAUNCHER/`, `LAUNCH/` and
`TOOLS/` are the Windows launcher and asset tools, not the game. None are on the
`-DWIN32` path. **That is 147,000 lines that need no attention at all** — the
single largest thing to know before estimating this work.

---

## Tweak — the game code

277 `.CPP` files in `CODE/`. **156 compile cleanly for arm64 today**; run
`port/probe.sh` for the current figure and blocker list.

Of the 121 still failing, ~22 are multiplayer/WOL/serial and out of scope, so the
real remainder is **~99 files**. These are now a long tail of 1–2 file problems
with no shared-header wall behind them — the phase where one header fix unblocked
185 files at once is over. Remaining error classes, by frequency:

- undeclared identifiers (missing small shims) — the largest group
- incomplete types and member access into them
- `cast from pointer to smaller type loses information` — **genuine ILP32
  assumptions.** These are the ones to read carefully rather than cast away;
  each is a real 64-bit correctness question.
- out-of-line definitions not matching declarations
- more dependent-base lookups needing `this->`, as in `ftimer.h`

The game logic itself — units, buildings, AI, mission scripting, the rules
engine — needs no translation at all. It is ordinary C++ that happens to be old.
That is the reason porting is worth it rather than rebuilding.

---

## Since this was compiled: the archive

The "drop" findings here have been acted on. 1,032 files — `WWFLAT32/`, `VQ/`,
`IPX/`, `LAUNCHER/`, `LAUNCH/`, `TOOLS/`, the duplicate `OLD`/`TEST`/`SRCDEBUG`
subdirectories, and 15 dead assembly files — were **moved to `archive/`, not
deleted**, with original paths preserved and a restore manifest. `probe.sh`
reported 156/277 before and after, and the Metal backend still builds.

The working tree is now `CODE/`, `WIN32LIB/`, `WINVQ/` and `port/`.

One correction to the table above: `CODE/WINASM.ASM` was classified DROP, and
that held, but for a better reason than given. It exports
`Asm_Create_Palette_Interpolation_Table`, which is *not* modem code — it is
already superseded by a C implementation at `CODE/INTERPAL.CPP:147`, with the
assembly call commented out on line 150. See `port/WORKLIST.md` for why that
matters to the whole TRANSLATE bucket.

## Open questions

1. **Which text rasteriser and which LCW codec are actually linked?** Four and
   two candidates respectively. Resolve before writing duplicates.
2. **Save-game compatibility.** `_MAX_FNAME`/`_MAX_EXT` were set to Watcom's
   Win32 values (256) to reproduce the original layout, but whether any type
   class carrying those arrays is serialised has not been audited.
3. **Is the MPEG cutscene path wanted at all?** The DLL was never released. The
   `.VQA` path is recoverable; matching the original MPEG playback is a rebuild
   with no reference.
4. **Case-sensitive filesystems.** This tree mixes `FUNCTION.H` and `function.h`
   and only builds because macOS volumes are case-insensitive by default.
5. **Still no version control.** Changes so far have touched 80–146 files at a
   time. This should be a git repository before the assembly work starts.
