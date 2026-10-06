# Command & Conquer: Red Alert — native macOS (Apple Silicon) port

This is a port of the 1996 *Command & Conquer: Red Alert* source code, released
by Electronic Arts, to run **natively on Apple Silicon Macs**. It renders with
**Metal**, takes input and windows from **Cocoa**, and plays sound through
**Core Audio**. There is no emulation layer, no Wine or CrossOver, no SDL, and
no Rosetta: the game is compiled for arm64.

The original game code is kept as Westwood wrote it wherever possible. The
Windows APIs it calls (DirectDraw, DirectSound, the Win32 message loop, timers,
files) are provided by a small compatibility layer over the native backend.
Westwood's x86 assembly has been translated to C and checked byte for byte
against the original assembly running under an x86 emulator. The one routine
missing from the source release, the decoder for the 640×400 start movie, was
written from the published file format and checked against it.

You need your own copy of the game's data files. The C&C Ultimate Collection is
sold on [EA App](https://www.ea.com/en-gb/games/command-and-conquer/command-and-conquer-the-ultimate-collection/buy/pc)
and [Steam](https://store.steampowered.com/bundle/39394/Command__Conquer_The_Ultimate_Collection/).

## Status

**Single-player is playable.** All campaigns work:

- Allied and Soviet (the base game)
- Counterstrike
- Aftermath
- the secret campaign

Also working:

- the intro, briefing and win movies, with sound;
- music and sound effects;
- the mission-select map and score screens;
- building, production and combat;
- **skirmish** against the computer (Multiplayer Game → Skirmish).

Network multiplayer (LAN, internet, modem) is not ported. It will be if enough
people ask for it; see [Multiplayer](#multiplayer) below.

## Download and play

1. Download **`Red Alert.zip`** from the
   [Releases page](https://github.com/TFrey98/CnC_Red_Alert-main/releases) and
   unzip it. You get a `Red Alert` folder with the app and
   **`READ ME FIRST.txt`**.
2. Get the game files from your own copy of the game. This download does not
   include them. They come with the C&C Ultimate Collection on Steam or the EA
   App:
   - **Installed on this Mac with CrossOver or Whisky:** nothing to do; the app
     finds them.
   - **Installed on a Windows PC:** copy the game's install folder (in Steam:
     right-click the game → Manage → Browse local files) into the `Red Alert`
     folder, next to the app.
3. Open `Red Alert.app`. The first time, macOS asks you to confirm an app that
   is not from the App Store. On macOS 15 and later: click Done, then System
   Settings → Privacy & Security → **Open Anyway**. On macOS 13 and 14:
   Control-click the app → Open.

`READ ME FIRST.txt` (source: [`port/release/`](port/release/READ%20ME%20FIRST.txt))
has the full steps, including how to get the files with no Windows PC.

Needs an Apple Silicon Mac (M1 or later) with macOS 13 Ventura or later.

## Building from source

### Requirements

- An Apple Silicon Mac, macOS 13 or later.
- Xcode or the Xcode Command Line Tools (`clang`, the macOS SDK with Metal).
- Python 3 (used by the build scripts).
- The game data from the Ultimate Collection / Steam release:
  - `REDALERT.MIX`, `HIRES1.MIX`, `LORES1.MIX`;
  - `MAIN1.MIX` – `MAIN4.MIX` (the four discs);
  - `EXPAND.MIX` (Counterstrike) and `EXPAND2.MIX` (Aftermath);
  - `REDALERT.INI`.

### Building the app

The build is driven by shell scripts; there is no Xcode project.

```sh
port/build-app.sh
```

This compiles the game and writes `build/Red Alert.app` and the release zip,
`build/Red Alert.zip`, which unpacks to a `Red Alert` folder holding the app
and `READ ME FIRST.txt`. The app contains only the game executable, its icon
(Westwood's original) and the license. **The game data is not included**:
players supply their own copy. To publish a release, upload
`build/Red Alert.zip` on the GitHub Releases page.

### Running the app

Open `Red Alert.app`. It finds the game data by itself if the game is
installed in one of the usual places:

- the Steam or EA App release in a CrossOver, Whisky or Wine bottle, for
  example
  `~/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/Command & Conquer Red Alert`;
- a native Steam download (`~/Library/Application Support/Steam/steamapps/common`);
- the folder the app itself is in (the release zip's layout). This also works
  when macOS runs a freshly downloaded app from a hidden copy ("app
  translocation"): the app looks next to where the player put it.

Otherwise it asks once for the folder that holds the game data
(`REDALERT.MIX`, `MAIN1.MIX` …); press Shift-Command-G in the picker to type a
path. The folder is remembered. To choose a different one, hold **Option**
while opening the app.

Saved games and settings are written to the data folder, as the original game
did with its install folder, so the folder must be writable.

The four discs are found automatically when `MAIN1.MIX` – `MAIN4.MIX` are
present, so the game never asks for a CD.

The app is signed ad hoc, not with an Apple Developer ID. A copy you build
yourself opens normally. A copy downloaded from the internet is blocked by
Gatekeeper the first time: open it, then go to System Settings → Privacy &
Security and choose **Open Anyway**.

### Running without the app

`port/link-census.sh` builds only the executable, at
`$TMPDIR/ra-link/redalert`. Give it the data folder in `RA_DATA_DIR`, put it in
the data folder, or let it ask as the app does:

```sh
RA_DATA_DIR="/path/to/Red Alert data" "$TMPDIR/ra-link/redalert"
```

Checks and tests:

```sh
port/tests/run.sh            # data-path and assembly-translation tests
port/build-backend.sh        # the Metal/Cocoa backend on its own
port/layout-check.sh         # struct layouts that differ between files (after link-census.sh)
RA_SANITIZE=1 port/link-census.sh   # an AddressSanitizer build of the whole game
```

`port/PORTING.md` explains how the port works and records every fix and why it
was needed. `port/PROGRESS.md` tracks current work.

## Features still to implement

- **Display options.** The game runs in a resizable window at its original
  640×400. There is no full-screen mode or choice of scaling filter yet.
- **Keyboard layouts.** Key-to-character translation assumes a US layout, so
  typing names and messages on other layouts may give the wrong characters.
- **The scenario editor** and Westwood's other tools are not part of the build.

## Multiplayer

Skirmish against the computer works. Network play does not: LAN, internet,
modem/serial and Westwood Online are stubbed out (`CODE/NETSTUB.CPP`), and the
Modem/Serial button says so.

**Network multiplayer will be ported if enough people ask for it.** If you want
it, please open an issue or add your vote to an existing one. The groundwork is
known: Westwood's UDP transport (`WSPUDP.CPP`) and LAN lobby (`NETDLG.CPP`) are
in the source, and `port/PROGRESS.md` lists the steps. It would be Mac to Mac
only, not with the original Windows game.

## Known bugs

- **The window can stay black at launch until clicked.** The game waits for
  the window to be activated before it starts, as Windows always did. If you
  launch from a terminal while another app is in front, macOS may not activate
  it until you click the window.
- **Switching away from the game and back** was recently fixed (the cursor
  could stop following the mouse) and has had limited testing. Please report
  anything odd after using another app.
- **Save and load, and the Counterstrike and Aftermath missions**, have had less
  testing than the main campaigns.
- **Westwood's own bugs are kept on purpose** where they change gameplay
  behaviour, so the game plays as it shipped. For example, an alliance-clearing
  typo in `HOUSE.CPP` is left as it was, and so is a check in `TECHNO.CPP`
  meant to make the AI target only fake buildings, which instead discards every
  building. Bugs that crashed or corrupted memory have been fixed and are
  noted in `port/PORTING.md`.

## License

The original source and this port are licensed under the GPL v3, with
additional terms. See [LICENSE.md](LICENSE.md). Electronic Arts' original README
for the source release is kept at
[`archive/README.original.md`](archive/README.original.md).
