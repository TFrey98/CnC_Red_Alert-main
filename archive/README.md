# archive/

**Nothing here is deleted, and nothing here is gone.** This directory holds the
1,054 files that the native arm64 / Metal port does not build, moved out of the
way so the working tree shows only what is actually in play.

The original paths are preserved exactly. `archive/WWFLAT32/...` was
`WWFLAT32/...`. Restoring anything is a `mv` back.

`manifest.json` is the machine-readable record: every entry with its original
path, file count, and the reason it was moved.

## Restoring

One entry:

```sh
mv archive/WWFLAT32 ./WWFLAT32
```

Everything, from the repo root:

```sh
python3 - <<'EOF'
import json, os, shutil
for e in json.load(open('archive/manifest.json')):
    src = os.path.join('archive', e['path'])
    if os.path.exists(src):
        os.makedirs(os.path.dirname(e['path']) or '.', exist_ok=True)
        shutil.move(src, e['path'])
EOF
```

After restoring or moving anything, run `port/probe.sh` and
`port/build-backend.sh`. The archiving was verified not to change either:
**156/277 translation units compiled clean before and after**, and the backend
still builds.

## What was moved, and why

### Whole directories (17)

| Path | Files | Reason |
|---|---|---|
| `WWFLAT32` | 400 | DOS-era twin of WIN32LIB. Its one live-looking reference, CODE/KEY.H:40, is inside #ifndef WIN32 and therefore dead in this build. |
| `VQ` | 183 | DOS twin of WINVQ. Not on the include path (-I uses WINVQ/INCLUDE). |
| `WIN32LIB/SRCDEBUG` | 169 | Debug-build duplicates of WIN32LIB sources. |
| `IPX` | 52 | 16-bit DOS IPX. Multiplayer, out of scope. |
| `LAUNCHER` | 47 | Windows launcher application, not the game. |
| `WINVQ/VQA32/OLD` | 30 | Superseded VQA decoder sources. |
| `TOOLS` | 25 | Win32 .EXE asset tools (MIXFILE, FONTMAKE, ...). Useful as reference for file formats; not buildable or needed here. |
| `WIN32LIB/AUDIO/OLD` | 25 | Superseded audio sources. |
| `WIN32LIB/KEYBOARD/OLD` | 21 | Superseded keyboard sources. |
| `WIN32LIB/EXAMPLE` | 11 | Library usage examples. |
| `WIN32LIB/MEM/MSVC` | 11 | MSVC-specific duplicate of MEM_COPY.ASM. |
| `WIN32LIB/MEM/OLDMEM` | 10 | Superseded memory sources. |
| `WIN32LIB/KEYBOARD/TEST` | 9 | Library tests; its ../mouse.h include only resolves inside this dir. |
| `WIN32LIB/DRAWBUFF/TEST` | 9 | Library tests; its ../gbuffer.h include only resolves inside this dir. |
| `WIN32LIB/KEYBOARD/OLDTEST` | 7 | Superseded keyboard tests. |
| `WIN32LIB/WW_WIN/OLD` | 6 | Superseded windowing sources. |
| `LAUNCH` | 2 | Windows launcher support, not the game. |

### Individual files (37)

| Path | Reason |
|---|---|
| `CODE/KEYFBUFF.ASM` | DOS variant; CODE/MAKEFILE builds 2KEYFBUF.OBJ under !ifdef WIN32. |
| `CODE/TXTPRNT.ASM` | DOS variant; 2TXTPRNT.OBJ is the Win32 one. |
| `CODE/SUPPORT.ASM` | DOS variant; 2SUPPORT.ASM is the Win32 one. |
| `CODE/IPXREAL.ASM` | DOS IPX real mode. Multiplayer, out of scope. |
| `CODE/IPXPROT.ASM` | DOS IPX protected mode. Multiplayer, out of scope. |
| `WINVQ/VQM32/XMODE.ASM` | VGA Mode X. No such path under Metal. |
| `WINVQ/VQM32/VESABUF.ASM` | VESA banked framebuffer. |
| `WINVQ/VQM32/MCGABUF.ASM` | MCGA framebuffer. |
| `WINVQ/VQM32/PORTIO.ASM` | Direct x86 port I/O. |
| `WINVQ/VQM32/MONO.ASM` | Monochrome MDA debug monitor output. |
| `WINVQ/VQA32/UNVQXMDE.ASM` | VQ decode straight to Mode X. |
| `WINVQ/VQA32/UNVQVESA.ASM` | VQ decode straight to VESA. |
| `WIN32LIB/PROFILE/APROFILE.ASM` | x86 cycle-counter profiler; Instruments replaces it. |
| `WIN32LIB/MEM/VMPAGEIN.ASM` | DOS virtual-memory paging helper. |
| `CODE/ALLOC.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/BMP8.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/CLASS.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/COMQUEUE.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/CONFDLG.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/DESCDLG.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/DPMI.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/FILE.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/INIBIN.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/INICODE.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/KEYBOARD.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/KEYFRAME.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/LZWOTRAW.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/MPLIB.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/MPLPC.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/NOSEQCON.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/NUMBER.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/SEQCONN.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/STUB.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/SURFACE.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/TARCOM.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/TEMP.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |
| `CODE/TURRET.CPP` | Not in the original WIN32 link set: CODE/MAKEFILE builds ra95.exe from OBJECTS + TECHFILES (tech.lib) + LIBFILES (jshell.lib), and this object is in none of them under !ifdef WIN32. |

## Corrections, and the rule this archive now follows

**The archive holds only files that nothing live depends on.** "Not in the
makefile" turned out not to be a safe test on its own -- neither `CODE/MAKEFILE`
nor the IDE project `RA95.PJT` is a complete record of what the shipped game
linked. Every candidate is now checked by reference: if live code calls
something defined only in that file, it stays in `CODE/`.

Nine files were archived and later **restored** under that rule:

| File | Why it came back |
|---|---|
| `CODE/WINASM.ASM` | Archived earlier as fully dead. Wrong: its `Asm_Interpolate` is called by `INTERPAL.CPP`, and `WINASM.OBJ` is in the makefile's Win32 link set. Only its *other* game export had a C replacement. |
| `CODE/LCWUNCMP.CPP` | A C implementation of `LCW_Uncompress` -- it replaces two copies of `LCWUNCMP.ASM` |
| `CODE/CSTRAW.CPP` | `CacheStraw`, used by live code |
| `CODE/RAND.CPP` | `Sim_IRandom`, called from `MPLAYER.CPP` |
| `CODE/CCMPATH.CPP`, `CCTEN.CPP`, `MPMGRD.CPP`, `MPMGRW.CPP`, `TENMGR.CPP` | multiplayer, but referenced by live code -- kept in `CODE/` tagged DROP, like the other multiplayer files |

Also kept out of the archive although the makefile never builds them:
`MAPEDSEL.CPP` (`#include`d as source by `MAPEDIT.CPP`), and `ADPCM.CPP` with its
`ITABLE.CPP`/`DTABLE.CPP` fragments -- a C version of the SOS ADPCM decoder.

## Why this is worth keeping rather than deleting

Three concrete reasons, not just caution:

1. **The DOS tree documents the algorithms.** `WWFLAT32/` and `VQ/` are earlier
   versions of the same routines that still need translating from assembly.
   Where a Win32 routine is hard to follow, its DOS twin is often clearer, and
   occasionally has a C version where the Win32 side has only assembly.

2. **`TOOLS/` documents the file formats.** The MIX, SHP, AUD and font packers
   are the authority on what the game's data files actually contain — useful if
   asset loading ever misbehaves, even though the `.EXE`s cannot run here.

3. **The archived assembly is the reference for its own replacement.** When
   `XMODE.ASM` or `MONO.ASM` turns out to have been doing something load-bearing
   that nothing else does, the source is right here.

A caution learned the hard way: `CODE/WINASM.ASM` was archived because its one
*obvious* game export, `Asm_Create_Palette_Interpolation_Table`, turned out to be
superseded by C in `INTERPAL.CPP`. Its other exports were not all checked, and
`Asm_Interpolate` is live -- so it was restored. **Check every exported symbol
before calling a mixed file dead, not just the first interesting one.**
