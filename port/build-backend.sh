#!/bin/zsh
#
# build-backend.sh -- compile the native macOS backend and prove the engine/
# backend boundary still holds.
#
# Run this after any change to port/backend/ or to the Win32 shim. It is fast,
# and it catches the failure mode that matters most here: a Win32 type or macro
# leaking into a translation unit that also sees Cocoa/Metal headers.

set -e
RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
OUT="${TMPDIR:-/tmp}/ra-backend"
mkdir -p "$OUT"

print "== 1. backend (.mm, Cocoa/Metal, NO Win32 shim) =="
clang++ $RA_OBJCXXFLAGS -c "${RA_ROOT}/port/backend/ra_metal.mm" -o "$OUT/ra_metal.o"
print "   ok"

print "== 2. offline shader compile (sanity; the app compiles it at runtime) =="
xcrun -sdk macosx metal   -c "${RA_ROOT}/port/backend/ra_palette.metal" -o "$OUT/ra_palette.air"
xcrun -sdk macosx metallib   "$OUT/ra_palette.air" -o "$OUT/ra_palette.metallib"
print "   ok"

print "== 3. boundary check: engine shim + ra_platform.h in one TU =="
cat > "$OUT/boundary.cpp" <<'CPP'
#include "windows.h"
#include "ra_platform.h"
/* BOOL must still be the engine's int, and min/max must still be macros. */
static BOOL check(void) { return (BOOL)max(1, min(2, 3)); }
int boundary_ok(void) { return check(); }
CPP
clang++ $RA_CXXFLAGS -I"${RA_ROOT}/port/backend" -c "$OUT/boundary.cpp" -o "$OUT/boundary.o"
print "   ok"

print "== 4. link =="
cat > "$OUT/main.cpp" <<'CPP'
extern int boundary_ok(void);
int main(void) { return boundary_ok() ? 0 : 1; }
CPP
clang++ $RA_CXXFLAGS -c "$OUT/main.cpp" -o "$OUT/main.o"
clang++ $RA_TARGET "$OUT/main.o" "$OUT/boundary.o" "$OUT/ra_metal.o" \
        -o "$OUT/ra-backend-smoke" $RA_FRAMEWORKS
print "   ok -> $OUT/ra-backend-smoke"
print ""
print "backend builds clean; boundary intact."
