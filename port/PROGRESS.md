# Progress: running the game (status as of 2026-10-05)

Where the runtime work stands, for picking it up again. `port/PORTING.md`
("Third pass" and "Fourth pass") is the permanent record of *why* each fix is
what it is. This file tracks what's working, what's open, and what to do next.

## Where the game is

- Boots to the menu natively (Metal and Cocoa) from the Steam data, with no
  disc prompts.
- Allied campaign: mission 1 plays to the end. The score screen, the
  mission-select map and mission 2 all work.
- In mission 2, building a Power Plant was checked by script and by the user:
  it produces, picks up, and enforces the placement rules.
- Movies (win and briefing) play.

## Fixed on 2026-10-05 (uncommitted)

| Symptom | Cause | Fix |
|---|---|---|
| Sound effects silent | `SOUNDIO.CPP` and `SOUNDINT.CPP` set a bare `#pragma pack(4)`, so they saw `LockedData` laid out differently from `SOUNDLCK.CPP`; `SoundVolume` read as 0 | Directives removed; new `port/layout-check.sh` finds this whole class |
| Game audio or movie audio decoding with the wrong struct | `ww_sos_adpcm.h` template instantiated for two different `_tagCOMPRESS_INFO`s with one mangled name | Header has internal linkage |
| Crash in `AnimClass` constructor (Soviet campaign; any big battle) | Pool-full `operator new` returns NULL; standard C++ constructs anyway | `-fcheck-new` in `port/flags.sh` |
| Library read a 1-byte `bool` as a 4-byte `BOOL` | `extern BOOL GameInFocus` in `SOUNDIO`, `GBUFFER`, `MOUSE` | Declared `bool` |
| Cursor lost after clicking out of the window and back | Cocoa sends mouse moves only to the active app; the game's cursor stayed where the pointer left | Always-active tracking area; pointer resync on activation |

From 2026-10-04 (committed by the user as 656a469): the disc mapping, `-fshort-enums`, the
sidebar/help read-only writes, shape offsets, WSA frame limit, scroll pacing,
PCX palette, and the audio chunk marker.

Checks after the last change:
- Link: 0 undefined and 0 duplicate symbols.
- `port/tests/run.sh`: all pass.
- Backend: builds clean.
- Probes: 234/254 and 97/106, unchanged.
- Worklist: unchanged.
- `port/layout-check.sh`: only the three known same-name clashes.

## Open, in priority order

1. **Retest by the user:**
   - **Focus and cursor.** Click out of the window and back, with and without
     clicking. The fix is verified only for emulated focus events and for real
     ones driven by AppleScript, not for real pointer movement.
   - **Effects.** They are audible now, by the numbers (`SoundVolume` = 255 at
     every play). Needs a listen.
   - **Music quality.** Needs a listen.
   - **Soviet campaign.** Should get past the `AnimClass` crash now.
2. **Startup waits for focus.** The game blocks in `INIT.CPP` until it gets
   `WM_ACTIVATEAPP`. Launched from a terminal while another app has focus,
   macOS may refuse activation, so the window sits black until clicked.
   Scripted runs work around this with an implicit `activate`. Consider sending
   the initial activate when the window is shown, as Windows did.
3. **Latent:** library `TickCount` (`TimerClass`) aliases the game's
   `TTimerClass` object. Only dead-stripped functions use it now. If more of
   `WIN32LIB/MEM` or `PALETTE` gets linked, give the library its own symbol.
4. Intro movie (`PlayIntro=yes`) path not yet checked.
5. `.app` bundle build script with a saved data-folder setting.
6. Unanswered question from earlier: whether to clamp the two original
   over-reads (`Asm_Interpolate_Line_Interpolate`, the predator shimmer below
   the view).

## Testing without playing

- **Scripted input:** `RA_INPUT_SCRIPT=<file>` (permanent; documented at the
  top of `port/compat/win32_window.cpp`). Timed `move`, `click`, `key`,
  `activate`, `deactivate`, `quit`. Real input and focus are ignored while it
  runs, except closing the window.
- **Known coordinates (640×400 framebuffer):**
  - main menu "Start New Game": (320,232); difficulty OK: (496,252);
    "Allies": (256,226);
  - mission 2 sidebar Power Plant: (530,200);
  - a valid placement is beside the Construction Yard and clear of units.
- **Timing gotchas:**
  - Esc in a running mission opens the Options (pause) menu, so only send Esc
    during movies.
  - The score screen waits for name entry; Enter continues.
  - A build takes about 7–10 s after the click registers, but mission-start
    timing varies, so leave margin.
- **Jumping to a scenario:** a temporary hook (not committed) is useful:
  at the top of `Main_Loop()`, once,
  `Scen.Set_Scenario_Name(getenv("RA_TEST_SCEN")); Start_Scenario(Scen.ScenarioName, false);`.
  Tag such hooks `PORT-DEBUG-TEMP` and strip them before stopping.
- **Frame dumps:** `RA_DUMP_FRAMES=<dir> RA_DUMP_EVERY=<n>`; frames are 8-bit
  BMPs.
- **Layout check:** `port/layout-check.sh` (after `port/link-census.sh`).

## Housekeeping

- The stray `CODE/.!18355!CONQUER.CPP` (an editor or sync snapshot) is
  archived at `archive/CODE/.!18355!CONQUER.CPP`, with a manifest entry.
- Test windows open on screen; scripted runs ignore clicks, but closing the
  window ends the run.
