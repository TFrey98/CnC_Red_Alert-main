# Progress: running the game (status as of 2026-10-05, afternoon)

Where the runtime work stands, for picking it up again. `port/PORTING.md`
("Third" to "Fifth pass") is the permanent record of *why* each fix is what it
is. This file tracks what's working, what's open, and what to do next.

## Where the game is

- Boots natively (Metal and Cocoa) from the Steam data, with no disc prompts.
  The Westwood logo and the opening cinematic play.
- Allied campaign: mission 1 to the end, then the score screen, mission-select
  map and mission 2. Briefing movies play (two before each mission; Esc skips).
- Building works: produce, pick up, place with the game's placement rules.
  Confirmed by script and by the user.
- A full AddressSanitizer session of mission 2 (menu, briefings, production,
  combat, unit moves; 129 s) reports zero memory errors.

## Uncommitted (since bb8ecdc)

| Symptom | Cause | Fix |
|---|---|---|
| Every building placement refused | PLACE event built by the `int` constructor; its cell overlays `Place.Cell` only under Watcom's `/zp1` | `CODE/EVENT.H` packed (1), `static_assert` on the overlap |
| Crash in `Find_Path` (stack buffer overflow) | END written one past a full 302-entry move list (original bug) | One slot of headroom, limit unchanged (`FINDPATH.CPP`) |
| "ENGLISH.VQA error 14"; movies not playing right | `iffsize` uninitialised when the loader resumes after an audio "sleep" | Re-derived from the saved chunk header (`WINVQ/VQA32/LOADER.CPP`) |
| Crash closing the window during a movie | Sound freed while the movie's audio timer ran | `Prog_End` stops movie audio first (`VQA_Port_Stop_Any_Audio`) |
| Weapons pointing into freed memory | Rules pools rebuilt for AFTRMATH.INI; Watcom happened to reuse the block | `Set_Heap` keeps storage at the same size (`HEAP.CPP`) |
| Library and game `TickCount` merged into one object | Watcom's type-encoded symbols kept them apart | `-DTickCount=WWLib_TickCount` for library compiles |
| LCW decoding past its buffer | The build used Westwood's C decoder, which ignores the length | `WIN32LIB/IFF/LCWUNCMP.CPP`, translated from the shipped assembly, verified (600 cases vs both copies) |
| Smaller sanitizer findings | `MissionControl[-1]`; unbounded bit scan; overlapping `strcpy`/`strncpy`/`memcpy`; straw destruction order | See `PORTING.md`, "Fifth pass" |

Checks after the last change:
- Link: 0 undefined and 0 duplicate symbols.
- `port/tests/run.sh`: all pass (now including `lcw_uncomp`).
- Backend: builds clean.
- Build set: game 233 + libraries 98 (LCW moved from game to library).
- `port/layout-check.sh`: only the three known same-name clashes.

## Open, in priority order

1. **Briefing movies play "almost properly"** (user report): get specifics from
   the user (picture, sound, timing?).
2. **Retest by the user:** Soviet campaign (past the old `AnimClass` crash?),
   music and effects by ear, focus/cursor with real pointer movement.
3. **Startup waits for focus.** The game blocks in `INIT.CPP` until it gets
   `WM_ACTIVATEAPP`. Launched while another app has focus, macOS may refuse
   activation and the window sits black until clicked. Consider sending the
   initial activate when the window is shown, as Windows did.
4. **Audit for other `/zp1` union overlaps** (as EventClass had). Dump record
   layouts with and without `-fpack-struct=1`, list unions whose members move,
   then read how each is used.
5. **More sanitizer coverage:** run `RA_SANITIZE=1` sessions on other missions,
   the Soviet campaign, saving and loading, and the remaining menus.
6. `.app` bundle build script with a saved data-folder setting.
7. Unanswered question from earlier: whether to clamp the two original
   over-reads (`Asm_Interpolate_Line_Interpolate`, the predator shimmer below
   the view).

## Testing without playing

- **Scripted input:** `RA_INPUT_SCRIPT=<file>` (documented at the top of
  `port/compat/win32_window.cpp`). Timed `move`, `click`, `key`, `activate`,
  `deactivate`, `quit`. Real input and focus are ignored while it runs, except
  closing the window.
- **Sanitizer builds:** `RA_SANITIZE=1 port/link-census.sh` builds everything
  with ASan (recover mode, `-g`). Run with
  `ASAN_OPTIONS=halt_on_error=0:abort_on_error=0` to collect every report.
  Rebuild without it afterwards, since the binary at `$TMPDIR/ra-link/redalert`
  is whichever was built last.
- **Timeline that works (640×400 coordinates):**
  - Esc at 2 s and 4 s skips the logo; main menu "Start New Game" (320,232) at
    8 s; difficulty OK (496,252) at 11 s; "Allies" (256,226) at 14 s.
  - Esc at 17 s and 20 s skips the two briefing movies.
  - Never send Esc in a running mission: it opens the Options menu.
  - The score screen waits for name entry; Enter continues.
  - Mission 2: Power Plant cameo (530,200); a placement that always works is
    (366,256), directly below the Construction Yard's apron. A build takes
    7–10 s once the click registers, but mission start varies, so leave margin.
- **Jumping to a scenario:** a temporary hook (not committed), at the top of
  `Main_Loop()`, once:
  `Scen.Set_Scenario_Name(getenv("RA_TEST_SCEN")); Start_Scenario(Scen.ScenarioName, false);`.
  Tag such hooks `PORT-DEBUG-TEMP` and strip them before stopping.
- **Frame dumps:** `RA_DUMP_FRAMES=<dir> RA_DUMP_EVERY=<n>`; 8-bit BMPs.
  Static screens present no frames, so move the pointer to force some.
- **Layout check:** `port/layout-check.sh` (after `port/link-census.sh`).

## Housekeeping

- The stray `CODE/.!18355!CONQUER.CPP` is archived at
  `archive/CODE/.!18355!CONQUER.CPP`, with a manifest entry.
- Test windows open on screen; closing one ends that run (it shows up as a run
  that stopped early).
