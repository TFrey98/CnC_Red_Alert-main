#!/bin/zsh
#
# probe.sh -- compile every game translation unit and report how many build
# cleanly for arm64, plus the first error in each that does not.
#
# This is the port's progress meter. Run it after any change to see whether the
# clean count moved and what the next-largest blocker is.

RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/ra-probe"
rm -rf "$OUT"; mkdir -p "$OUT"

# Compile exactly the translation units the shipped WIN32 game was built from,
# not every .CPP that happens to be in CODE/. The list is derived from
# CODE/MAKEFILE by port/gen-build-set.py; regenerate it if the makefile or the
# set of reconstructed files changes. Files outside it were never part of the
# game (see archive/README.md), and a few (ITABLE.CPP, DTABLE.CPP,
# MAPEDSEL.CPP) are fragments that are only valid when #included elsewhere.
SET="${RA_ROOT}/port/build-set.txt"
cd "${RA_ROOT}/CODE"
xargs -P 8 -I{} "${RA_ROOT}/port/probe-one.sh" "$OUT" "{}" < "$SET" > "$OUT/results.txt"

total=$(wc -l < "$SET" | tr -d ' ')
ok=$(grep -c '^OK' "$OUT/results.txt")
print ""
print "=== arm64 clean: ${ok} / ${total} ==="
print ""
print "=== next blockers (by number of files affected) ==="
for f in "$OUT"/*.log(N); do
  grep -m1 "error:" "$f" 2>/dev/null
done | sed -E 's/^(.*):[0-9]+:[0-9]+: (fatal )?error: /\1: /' \
     | sed -E "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn | head -15
print ""
print "logs: $OUT"
