#!/bin/zsh
#
# build-app.sh -- build the game and package it as a Mac app.
#
#   port/build-app.sh
#
# Writes "build/Red Alert.app" and "build/Red Alert.zip" (the app, zipped for
# sharing). The app holds only the executable, its icon and the license: the
# game data is not included. At first launch the game asks for the folder that
# holds it and remembers the choice (see port/compat/win32_main.cpp; hold
# Option while launching to choose again).
#
# The app is signed ad hoc, not with a Developer ID, so a copy downloaded from
# the internet is quarantined by Gatekeeper; see README.md.

set -e
RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
L="${TMPDIR:-/tmp}/ra-link"

if [[ -n "$RA_SANITIZE" ]]; then
  print "build-app.sh: RA_SANITIZE is set; an AddressSanitizer build is not for packaging." >&2
  exit 1
fi

# 1. The game itself.
"$RA_ROOT/port/link-census.sh"
if ! grep -q "undefined symbols: 0 .*duplicate symbols: 0 .*failed to compile: 0" <(python3 "$RA_ROOT/port/link-census.py" "$L") \
   || [[ ! -x "$L/redalert" ]]; then
  print "build-app.sh: the game did not link cleanly; see $L/link.txt" >&2
  exit 1
fi

# 2. The bundle.
APP="$RA_ROOT/build/Red Alert.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$L/redalert" "$APP/Contents/MacOS/redalert"
python3 "$RA_ROOT/port/make-icon.py" "$RA_ROOT/CODE/REDALERT.ICO" "$APP/Contents/Resources/RedAlert.icns"
cp "$RA_ROOT/LICENSE.md" "$APP/Contents/Resources/LICENSE.md"

# Build number: commits on this branch, so each committed build counts up.
BUILD=$(git -C "$RA_ROOT" rev-list --count HEAD 2>/dev/null || print 0)

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleName</key>                  <string>Red Alert</string>
	<key>CFBundleDisplayName</key>           <string>Red Alert</string>
	<key>CFBundleIdentifier</key>            <string>io.github.tfrey98.redalert</string>
	<key>CFBundleExecutable</key>            <string>redalert</string>
	<key>CFBundleIconFile</key>              <string>RedAlert</string>
	<key>CFBundlePackageType</key>           <string>APPL</string>
	<key>CFBundleInfoDictionaryVersion</key> <string>6.0</string>
	<key>CFBundleShortVersionString</key>    <string>3.03</string>
	<key>CFBundleVersion</key>               <string>${BUILD}</string>
	<key>LSMinimumSystemVersion</key>        <string>13.0</string>
	<key>LSApplicationCategoryType</key>     <string>public.app-category.strategy-games</string>
	<key>NSHighResolutionCapable</key>       <true/>
	<key>NSHumanReadableCopyright</key>      <string>Command &amp; Conquer Red Alert © 1996 Westwood Studios / Electronic Arts. Released under the GPL v3 with additional terms.</string>
</dict>
</plist>
PLIST
plutil -lint "$APP/Contents/Info.plist" > /dev/null

# 3. Sign (ad hoc) and check.
codesign --force --sign - "$APP"
codesign --verify --strict "$APP"

# 4. A zip for sharing; ditto keeps the bundle's metadata and signature intact.
rm -f "$RA_ROOT/build/Red Alert.zip"
ditto -c -k --keepParent "$APP" "$RA_ROOT/build/Red Alert.zip"

print "built: $APP"
print "       $RA_ROOT/build/Red Alert.zip"
