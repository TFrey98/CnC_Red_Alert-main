# Worklist — what needs doing, per file

Generated from a live `port/probe.sh` run plus `port/asm-inventory.json`.
Machine-readable twin: **`port/worklist.json`**.

Regenerate after any batch of fixes; the CODE tags are derived from actual
compiler output, not from a static list, so they stay honest automatically.

## Tags

| Tag | Meaning | Count |
|---|---|---|
| **DONE** | Compiles clean for arm64 today. No action. | 156 files |
| **TWEAK** | Small mechanical source edits to satisfy clang. | 99 files |
| **DROP** | Multiplayer / online / serial. Out of scope; left in place because the rest of `CODE/` still includes its headers. | 22 files |
| **TRANSLATE** | x86 assembly → portable C. | 69 files, 23,783 lines |
| **NATIVE** | Needed, but reimplemented on a macOS framework. | 5 files, 3,701 lines |
| **REBUILD** | No usable source, or concept doesn't survive the move. | 3 files, 450 lines |

## Why nothing in `CODE/` was moved to the archive

The 22 DROP files are genuinely out of scope, but `CODE/` is one interdependent
unit: `FUNCTION.H` includes `session.h`, `tcpip.h`, `ipxmgr.h` and `nullmgr.h`
unconditionally, so removing the `.CPP` files would not remove the headers, and
removing the headers would break all 277 files. They are tagged, not moved. They
also cost nothing — they are simply never compiled into a single-player build.

---

## TWEAK — 99 files

Grouped by first compiler error, because the same fix usually clears a whole
group. Work the largest group first.

### use of undeclared identifier 'X'  — 19 file(s)

`AIRCRAFT.CPP`, `CDFILE.CPP`, `COMINIT.CPP`, `DIBUTIL.CPP`, `DYNAVEC.CPP`, `IOMAP.CPP`, `KEY.CPP`, `KEYBOARD.CPP`, `MAP.CPP`, `NOSEQCON.CPP`, `NUMBER.CPP`, `RAWFILE.CPP`, `ROTBMP.CPP`, `RULES.CPP`, `SEQCONN.CPP`, `SURFACE.CPP`, `TARCOM.CPP`, `TOOLTIP.CPP`, `VORTEX.CPP`

### no matching function for call to 'X'  — 10 file(s)

`CONFDLG.CPP`, `DIALOG.CPP`, `EDIT.CPP`, `INIT.CPP`, `LIST.CPP`, `MSGBOX.CPP`, `READLINE.CPP`, `SENDFILE.CPP`, `STATBTN.CPP`, `TEXTBTN.CPP`

### member access into incomplete type 'X'  — 9 file(s)

`CONQUER.CPP`, `EXPAND.CPP`, `GOPTIONS.CPP`, `LOADDLG.CPP`, `MENUS.CPP`, `NETDLG.CPP`, `OPTIONS.CPP`, `SCENARIO.CPP`, `TACTION.CPP`

### use of undeclared identifier 'X'; did you mean 'X'?  — 6 file(s)

`COMQUEUE.CPP`, `LZWOTRAW.CPP`, `STARTUP.CPP`, `THEME.CPP`, `VERSION.CPP`, `WINSTUB.CPP`

### out-of-line definition of 'X' does not match any declaration in 'X'  — 5 file(s)

`BUILDING.CPP`, `FOOT.CPP`, `INFANTRY.CPP`, `TECHNO.CPP`, `UNIT.CPP`

### cast from pointer to smaller type 'X' loses information  — 4 file(s)

`2KEYFRAM.CPP`, `KEYFRAME.CPP`, `LCW.CPP`, `LCWUNCMP.CPP`

### unknown type name 'X'  — 4 file(s)

`BMP8.CPP`, `CCDDE.CPP`, `DIBFILE.CPP`, `TEMP.CPP`

### cannot decrement expression of type bool  — 4 file(s)

`BULLET.CPP`, `FACTORY.CPP`, `REINF.CPP`, `VESSEL.CPP`

### non-object type 'X' is not assignable  — 4 file(s)

`CELL.CPP`, `SAVELOAD.CPP`, `SPECIAL.CPP`, `TEAMTYPE.CPP`

### variable has incomplete type 'X'  — 3 file(s)

`ALLOC.CPP`, `MAPSEL.CPP`, `SIDEBAR.CPP`

### no matching member function for call to 'X'  — 3 file(s)

`CCINI.CPP`, `INI.CPP`, `STATS.CPP`

### 'X' can only be specified inside the class definition  — 3 file(s)

`DDE.CPP`, `SLIDER.CPP`, `TXTLABEL.CPP`

### cannot assign to non-static data member within const member function '  — 2 file(s)

`ANIM.CPP`, `OBJECT.CPP`

### no viable overloaded 'X'  — 2 file(s)

`DISPLAY.CPP`, `INTRO.CPP`

### template specialization requires 'X'  — 2 file(s)

`HOUSE.CPP`, `INT.CPP`

### conflicting types for 'X'  — 2 file(s)

`MISSION.CPP`, `MIXFILE.CPP`

### constant expression evaluates to 253 which cannot be narrowed to type   — 1 file(s)

`COORD.CPP`

### expected 'X', 'X', 'X', or 'X'  — 1 file(s)

`DPMI.CPP`

### reference to non-static member function must be called; did you mean t  — 1 file(s)

`EVENT.CPP`

### cannot initialize a variable of type 'X' with an rvalue of type 'X'  — 1 file(s)

`FIXED.CPP`

### template parameter list matching the non-templated nested type 'X' sho  — 1 file(s)

`GLOBALS.CPP`

### C-style cast from 'X' to 'X' is not allowed  — 1 file(s)

`IOOBJ.CPP`

### value of type 'X' is not contextually convertible to 'X'  — 1 file(s)

`JSHELL.CPP`

### comparison between pointer and integer ('X' and 'X')  — 1 file(s)

`LAYER.CPP`

### constant expression evaluates to 218 which cannot be narrowed to type   — 1 file(s)

`MONOC.CPP`

### assigning to 'X' from 'X' discards qualifiers  — 1 file(s)

`PROFILE.CPP`

### a type specifier is required for all declarations  — 1 file(s)

`RADAR.CPP`

### incomplete type 'X' named in nested name specifier  — 1 file(s)

`RGB.CPP`

### constant expression evaluates to 192 which cannot be narrowed to type   — 1 file(s)

`SCORE.CPP`

### 'X' must return 'X'  — 1 file(s)

`STUB.CPP`

### constructor for 'X' must explicitly initialize the base class 'X' whic  — 1 file(s)

`TURRET.CPP`

### 'X' file not found  — 1 file(s)

`UDPADDR.CPP`

### invalid application of 'X' to an incomplete type 'X'  — 1 file(s)

`WRITEPCX.CPP`

---

## TRANSLATE — 69 files, 23,783 lines

| File | Lines |
|---|---|
| CODE/2KEYFBUF.ASM | 4848 |
| WINVQ/VQA32/UNVQBUFF.ASM | 1153 |
| WIN32LIB/SHAPE/DRAWSHP.ASM | 1128 |
| WIN32LIB/WSA/XORDELTA.ASM | 669 |
| WIN32LIB/DRAWBUFF/FILLQUAD.ASM | 669 |
| WIN32LIB/DRAWBUFF/STAMP.ASM | 600 |
| WIN32LIB/DRAWBUFF/SCALE.ASM | 570 |
| CODE/2SUPPORT.ASM | 564 |
| CODE/2TXTPRNT.ASM | 507 |
| WIN32LIB/DRAWBUFF/TXTPRNT.ASM | 502 |
| WIN32LIB/DRAWBUFF/DRAWLINE.ASM | 464 |
| WIN32LIB/DRAWBUFF/BITBLIT.ASM | 462 |
| WIN32LIB/FONT/TEXTPRNT.ASM | 436 |
| WIN32LIB/PALETTE/PAL.ASM | 410 |
| WINVQ/VQM32/DRAWCHAR.ASM | 395 |
| WINVQ/VQM32/HUFFDCMP.ASM | 391 |
| WINVQ/VQM32/AUDUNZAP.ASM | 375 |
| WIN32LIB/AUDIO/AUDUNCMP.ASM | 374 |
| WIN32LIB/SHAPE/DS_DSR.ASM | 341 |
| WIN32LIB/SHAPE/DS_DS.ASM | 341 |
| WINVQ/VQM32/PALETTE.ASM | 319 |
| WIN32LIB/DRAWBUFF/TOPAGE.ASM | 294 |
| WIN32LIB/IFF/LCWUNCMP.ASM | 292 |
| WIN32LIB/DRAWBUFF/TOBUFF.ASM | 292 |
| WIN32LIB/IFF/LCWCOMP.ASM | 286 |
| WIN32LIB/DRAWBUFF/STMPCACH.ASM | 284 |
| CODE/LCWCOMP.ASM | 284 |
| WIN32LIB/DRAWBUFF/FILLRECT.ASM | 275 |
| WIN32LIB/MISC/CLIPRECT.ASM | 269 |
| WINVQ/VQM32/LCWCOMP.ASM | 266 |
| WIN32LIB/SHAPE/DS_DR.ASM | 257 |
| WIN32LIB/SHAPE/DS_DN.ASM | 257 |
| WINVQ/VQM32/LCWUNCMP.ASM | 221 |
| WINVQ/VQM32/FILLRECT.ASM | 216 |
| WIN32LIB/MISC/FADING.ASM | 215 |
| WIN32LIB/DRAWBUFF/SHADOW.ASM | 210 |
| WIN32LIB/SHAPE/DS_TABLE.ASM | 187 |
| WIN32LIB/MEM/MEM_COPY.ASM | 184 |
| WINVQ/VQM32/TEXTPRNT.ASM | 178 |
| WIN32LIB/DRAWBUFF/REMAP.ASM | 175 |
| WIN32LIB/MISC/FACINGFF.ASM | 165 |
| WINVQ/VQAVIEW/INTERPAL.ASM | 162 |
| WIN32LIB/SHAPE/DS_LSS.ASM | 159 |
| WIN32LIB/SHAPE/DS_LSRS.ASM | 159 |
| WIN32LIB/MISC/SHAKESCR.ASM | 158 |
| WIN32LIB/MISC/FACING16.ASM | 148 |
| WIN32LIB/MISC/FACING8.ASM | 140 |
| WIN32LIB/MISC/REVERSE.ASM | 139 |
| WINVQ/VQM32/VB.ASM | 137 |
| CODE/COORDA.ASM | 134 |
| WINVQ/VQM32/CRC.ASM | 133 |
| WIN32LIB/IFF/PACK2PLN.ASM | 132 |
| WIN32LIB/DRAWBUFF/CLEAR.ASM | 130 |
| WIN32LIB/SHAPE/DS_LS.ASM | 118 |
| WIN32LIB/SHAPE/DS_LRS.ASM | 118 |
| WIN32LIB/MISC/RANDOM.ASM | 118 |
| WIN32LIB/DRAWBUFF/GETCLIP.ASM | 115 |
| WIN32LIB/MISC/CRC.ASM | 114 |
| WIN32LIB/SHAPE/DS_RSS.ASM | 110 |
| WIN32LIB/SHAPE/DS_RSRS.ASM | 110 |
| WIN32LIB/SHAPE/DS_RS.ASM | 110 |
| WIN32LIB/SHAPE/DS_RRS.ASM | 110 |
| WIN32LIB/DRAWBUFF/PUTPIX.ASM | 110 |
| WIN32LIB/FONT/SETFPAL.ASM | 106 |
| WIN32LIB/DRAWBUFF/GETPIX.ASM | 106 |
| WIN32LIB/DRAWBUFF/FTPUTPIX.ASM | 106 |
| WIN32LIB/DRAWBUFF/SZREGION.ASM | 101 |
| WIN32LIB/SHAPE/SHAPE.ASM | 94 |
| WIN32LIB/SHAPE/SETSHAPE.ASM | 81 |

## NATIVE — 5 files

| File | Lines |
|---|---|
| WINVQ/VQM32/SOSCODEC.ASM | 1271 |
| WIN32LIB/AUDIO/OLSOSDEC.ASM | 755 |
| WIN32LIB/AUDIO/SOSCODEC.ASM | 724 |
| WIN32LIB/KEYBOARD/WWMOUSE.ASM | 662 |
| WIN32LIB/PLAYCD/PLAYCD.ASM | 289 |

## REBUILD — 3 files

| File | Lines |
|---|---|
| CODE/CPUID.ASM | 185 |
| WIN32LIB/MISC/OPSYS.ASM | 150 |
| WIN32LIB/MISC/DETPROC.ASM | 115 |

---

## DROP (tagged in place) — 22 files

`CONNECT.CPP`, `EGOS.CPP`, `INTERNET.CPP`, `IPX.CPP`, `IPX95.CPP`, `IPXMGR.CPP`, `MPGSET.CPP`, `MPLAYER.CPP`, `MPLIB.CPP`, `MPLPC.CPP`, `MPMGRD.CPP`, `MPMGRW.CPP`, `NULLDLG.CPP`, `NULLMGR.CPP`, `PACKET.CPP`, `QUEUE.CPP`, `SESSION.CPP`, `TCPIP.CPP`, `WOL_GSUP.CPP`, `WSPIPX.CPP`, `WSPROTO.CPP`, `WSPUDP.CPP`

---

## A lead worth chasing before translating anything

`CODE/WINASM.ASM` was archived after discovering that its one game-relevant
export, `Asm_Create_Palette_Interpolation_Table`, is **already superseded by C
in this tree**: `CODE/INTERPAL.CPP:147` contains a full C implementation under
`#if (1)`, and the assembly call on line 150 is commented out. Westwood had
already done that translation themselves.

The assembly inventory was built by checking for same-named `.CPP` siblings,
which would not have caught this. **Before writing C for any of the
23,783 TRANSLATE lines, grep for its exported symbol
names across `CODE/` and `WIN32LIB/`** — some of that work may already exist,
commented out or behind an `#if`, exactly as this one was.

---

## DONE — 156 files

Listed in `port/worklist.json` only, to keep this file readable.
