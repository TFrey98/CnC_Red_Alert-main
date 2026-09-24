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

cd "${RA_ROOT}/CODE"
ls *.CPP | xargs -P 8 -I{} "${RA_ROOT}/port/probe-one.sh" "$OUT" "{}" > "$OUT/results.txt"

total=$(ls *.CPP | wc -l | tr -d ' ')
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
