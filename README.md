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
- building, production and combat.

Multiplayer is not ported yet (see below).

## Requirements

- An Apple Silicon Mac, macOS 13 or later.
- Xcode or the Xcode Command Line Tools (`clang`, the macOS SDK with Metal).
- Python 3 (used by the build scripts).
- The game data from the Ultimate Collection / Steam release:
  - `REDALERT.MIX`, `HIRES1.MIX`, `LORES1.MIX`;
  - `MAIN1.MIX` – `MAIN4.MIX` (the four discs);
  - `EXPAND.MIX` (Counterstrike) and `EXPAND2.MIX` (Aftermath);
  - `REDALERT.INI`.

## Building and running

The build is driven by shell scripts; there is no Xcode project.

```sh
port/link-census.sh          # compile the game and libraries, link the executable
```

The executable is written to `$TMPDIR/ra-link/redalert`. Run it with the data
folder in `RA_DATA_DIR`, or copy it into the folder that holds the data:

```sh
RA_DATA_DIR="/path/to/Red Alert data" "$TMPDIR/ra-link/redalert"
```

The four discs are found automatically when `MAIN1.MIX` – `MAIN4.MIX` are
present, so the game never asks for a CD.

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

- **Multiplayer.** LAN, internet, modem/serial and Westwood Online are stubbed
  out (`CODE/NETSTUB.CPP`); the menus are there, but games cannot be hosted or
  joined.
- **A proper Mac app.** The game is a bare executable. It needs an `.app`
  bundle, an icon, and a saved setting for where the game data lives, instead
  of `RA_DATA_DIR`.
- **Display options.** The game runs in a resizable window at its original
  640×400. There is no full-screen mode or choice of scaling filter yet.
- **Keyboard layouts.** Key-to-character translation assumes a US layout, so
  typing names and messages on other layouts may give the wrong characters.
- **The scenario editor** and Westwood's other tools are not part of the build.

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
