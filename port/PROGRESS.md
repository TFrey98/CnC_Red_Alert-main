# Progress: running the game (status as of 2026-10-04)

Where the runtime work stands, for picking it up again. `port/PORTING.md` is
the permanent record of *why* each fix is what it is. This file tracks what's
working, what's open, and what to do next.

## Where the game is

- Boots to the main menu natively (Metal and Cocoa), with the Steam data folder
  as the source. No disc prompts.
- Allied mission 1 is playable to the end. The win screen, the score screen and
  the mission-select map all work, and mission 2 loads.
- Movies (the win and briefing movies) play.

## Fixed in this round (all uncommitted)

| Symptom | Cause | Fix |
|---|---|---|
| Campaign asked for a disc | Steam ships the discs as `MAIN1`-`MAIN4.MIX`; this source expects a CD drive | `CODE/CONQUER.CPP`: `Port_Discs_Installed`, `Main_Mix_Name`; a requested disc is "inserted" if its `MAINn.MIX` is present. Also used in `INIT.CPP` |
| Crash starting a mission (sidebar) | In-place write to a string literal (read-only on macOS) | `CODE/SIDEBAR.CPP`: names are `char[][12]` |
| Crash in `Build_Frame` | Frame-offset table read into 8-byte `unsigned long` | `CODE/2KEYFRAM.CPP`: `uint32_t` |
| Crashes in map load, pathfinding, `Dir_To_32` | Watcom sized enums to fit (`DirType` 1 byte, etc.); the code relies on byte wrap and byte-sized copies | `-fshort-enums` in `port/flags.sh`; explicit widths in `MAP.CPP`, `OVERLAY.CPP`, `FINDPATH.CPP`, `FOOT.CPP` |
| Crash in tooltips | `const` array written through a cast | `CODE/HELP.H` / `HELP.CPP`: no longer `const` |
| Crash after winning (map select) | WSA frame limit used this build's `sizeof(SysAnimHeaderType)` (64) not the 32-bit one (43) | `WIN32LIB/WSA/WSA.CPP`: `SYSANIM_HEADER_SIZE_32` |
| Edge-scrolling far too fast; view flew into the black map corner (looked like a lost cursor) | `Sync_Delay`'s wait loop scrolled once per pass and spun thousands of times per frame | `CODE/CONQUER.CPP` `Sync_Delay`: one pass per 60 Hz tick |
| Score screen corrupted (white noise on plaques and bars) | PCX palette shifted as signed `char`: 168 became 234, then the +30% brightening capped it to white | `CODE/WINSTUB.CPP` and `WIN32LIB/IFF/LOADPCX.CPP`: unsigned shift |
| Music glitchy and restarting; effects silent (part 1) | Audio chunk marker read into 8-byte `long magic`: wrong size and garbage, so most chunks were rejected | `WIN32LIB/AUDIO/SOUNDINT.CPP`: `int32_t magic` |

Verification:
- Shape decoding was checked frame by frame (734 frames, including delta
  frames) against an independent Python decoder; all match.
- The enum sizes were confirmed with a compile-time check.
- The scroll, score-screen and audio-restart fixes were each confirmed by
  re-running and inspecting the dumped frames or a DirectSound trace.
- After the last change: link has 0 undefined and 0 duplicate symbols,
  `port/tests/run.sh` passes all tests, and the backend builds clean. The
  probes and worklist were last run before the WSA, PCX and audio fixes and
  were unchanged then (234/254, 97/106).

## Open, in priority order

1. **Sound effects are still silent.** `Play_Sample_Handle` gets volume 100,
   which is correct, but the buffer is set to -10000 (silent). The formula is
   `Convert_HMI_To_Direct_Sound_Volume((LockedData.SoundVolume * volume) / 256)`,
   so `LockedData.SoundVolume` must be 0 at that point. It's set to 255 in
   `SOUNDLCK.CPP` and only changed by `Set_Sound_Vol` (nothing calls it) and
   the swap in `File_Stream_Preload` (`SOUNDIO.CPP:462-469`). Next step: read
   it at runtime. lldb couldn't evaluate `LockedData.SoundVolume` because there
   is no debug info, so use a temporary `fprintf` or
   `memory read &LockedData`. Also check whether `Init_Locked_Data` runs after
   something else zeroes `LockedData`.
2. **Music quality after the magic fix:** not yet heard. The restart loop is
   gone (1,512 plays down to 42), but needs a listen.
3. **Can't build structures in mission 2** (user report). Not investigated.
   Check that the sidebar build icons respond to clicks, then that placement
   (`Passes_Proximity_Check`, the placement cursor) works.
4. **Mouse cursor lost after map select** (user report). The hide/show counter
   was traced through win → score → map → mission 2 and ends balanced at 0.
   It's most likely the over-fast scrolling throwing the view into the black
   corner, now fixed. Retest.
5. Other between-mission glitches the user saw: recheck once 1-4 are done.
6. Intro movie (`PlayIntro=yes`) appeared not to play at first boot; the win
   movie does play, so check the intro path.
7. Later: `.app` bundle build script with a saved data-folder setting.
8. Unanswered question from earlier: whether to clamp the two original
   over-reads (`Asm_Interpolate_Line_Interpolate`, the predator shimmer below
   the view).

## How to reproduce things without playing

These were temporary hooks, now removed. Re-add them when needed, tagged
`PORT-DEBUG-TEMP` so they're easy to strip.

- **Force a win at the first main-loop pass:** in `Main_Loop()`, call
  `Do_Win()` once when an env var is set. In `Map_Selection()`'s loop, set
  `done = 1; selection = 0;` after about 120 passes to auto-pick a spot.
- **DirectSound trace:** an env-gated `fprintf` in `port/compat/win32_dsound.cpp`
  on create, play, stop and SetVolume.
- **Frame dumps:** `RA_DUMP_FRAMES=<dir> RA_DUMP_EVERY=<n>`. A contact-sheet
  script (pure Python, no PIL) was in the session scratchpad; it's easy to
  rewrite (read 8-bit BMPs, tile at half size).
- **lldb:** `lldb --batch -s script` with
  `br command add 1 / bt 6 / continue / DONE` for caller statistics. An
  expression that crashes stops the batch, so use hooks for that instead.

## Housekeeping

- `CODE/.!18355!CONQUER.CPP` is a stray snapshot written by some editor or sync
  process (it contains an old debug hook). It's not built; safe to delete.
- The window opens on screen during test runs, and clicks in it are real input.
- `port/PORTING.md` documents fixes through the enum work ("Third pass").
  The WSA, scroll, PCX palette and audio-magic fixes above still need adding
  there.
