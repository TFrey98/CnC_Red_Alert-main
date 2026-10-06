# Progress: running the game (status as of 2026-10-05, evening)

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
- Skirmish: Multiplayer Game -> Skirmish -> setup dialog -> OK starts a game
  (Russia, 10000 credits, "A Path Beyond" by default). A 165 s ASan session
  reports zero memory errors. Modem/Serial shows a "not available" message.

## Uncommitted (since 724161d)

| Symptom | Cause | Fix |
|---|---|---|
| No Mac app; data folder only via `RA_DATA_DIR` | Not built yet | `port/build-app.sh` -> `build/Red Alert.app` + zip, ad hoc signed; icon from `REDALERT.ICO` (`port/make-icon.py`); data folder found automatically in CrossOver/Whisky/Wine bottles and native Steam, else a picker (`win32_main.cpp`), saved via `ra_dialog.mm`. Release zip = `Red Alert/` folder (app + `READ ME FIRST.txt` from `port/release/`) for drop-in game files; translocation-aware app folder. README: "Download and play", multiplayer "on request" |

Checks after the last change:
- Link: 0 undefined and 0 duplicate symbols.
- `port/tests/run.sh`: all pass (now including `lcw_uncomp`).
- Backend: builds clean.
- Build set: game 233 + libraries 98 (LCW moved from game to library).
- `port/layout-check.sh`: only the three known same-name clashes.

## Open, in priority order

0. **Network multiplayer: on hold** until enough players ask for it (README,
   "Multiplayer"). When it resumes, next steps (LAN over UDP, Mac to Mac): determinism
   harness (game CRC / record-playback on skirmish); Winsock shim
   (`WSAAsyncSelect` via the message pump, `getifaddrs` broadcast addresses);
   build `WSPROTO`/`WSPUDP`/`IPXMGR` with `WINSOCK_IPX`; port in the UDP
   address so two copies can run on one Mac; then `NETDLG.CPP` (the lobby).

1. **Movies:** user to confirm the start movie and briefings look smooth.
2. **Retest by the user:** Soviet campaign beyond mission 1,
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
6. ~~`.app` bundle~~ done (`port/build-app.sh`). Launched with `open` and
   nothing saved, it finds the CrossOver Steam data and plays the intro. The
   fallback folder picker is unconfirmed by hand (scripts cannot drive it).
   Unzipped release + game files dropped beside the app: reaches the menu.
   Untested: a real download (quarantine, Gatekeeper's Open Anyway, app
   translocation) -- needs a published release and a click-through.
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
- **Frame dumps are 640×480; script coordinates are 640×400.** Subtract 40
  from a y read off a dumped menu frame. Main menu: Multiplayer Game (320,278).
  Multiplayer menu: Modem/Serial (320,230), Skirmish (320,252). Skirmish
  dialog: OK (105,368). With Esc at 2, 4 and 6 s, the main menu is up by 12 s.
- **The app's saved data folder:** `defaults write io.github.tfrey98.redalert
  DataFolder -string "<path>"` (`-string`: the Steam path's parentheses break
  plain `defaults write`); `defaults delete io.github.tfrey98.redalert
  DataFolder` brings the picker back. Run
  `"build/Red Alert.app/Contents/MacOS/redalert"` directly to pass
  `RA_INPUT_SCRIPT` and friends.
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
