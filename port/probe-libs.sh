#!/bin/zsh
#
# probe-libs.sh -- compile every library translation unit (port/lib-build-set.txt)
# with that library's own flags, and report clean / total plus the top blockers.
# The library counterpart of probe.sh.

RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/ra-probe-libs"
rm -rf "$OUT"; mkdir -p "$OUT"
xargs -P 8 -I{} "${RA_ROOT}/port/probe-lib-one.sh" "$OUT" "{}" < "${RA_ROOT}/port/lib-build-set.txt" > "$OUT/results.txt"

total=$(wc -l < "${RA_ROOT}/port/lib-build-set.txt" | tr -d ' ')
ok=$(grep -c '^OK' "$OUT/results.txt")
print ""
print "=== libraries arm64 clean: ${ok} / ${total} ==="
for lib in WIN32LIB WINVQ; do
  t=$(grep -c " ${lib}/" "$OUT/results.txt"); o=$(grep -c "^OK ${lib}/" "$OUT/results.txt")
  print "    ${lib}: ${o} / ${t}"
done
print ""
print "=== next blockers (by number of files affected) ==="
for f in "$OUT"/*.log(N); do
  grep -m1 "error:" "$f" 2>/dev/null
done | sed -E 's/^(.*):[0-9]+:[0-9]+: (fatal )?error: /\1: /' \
     | sed -E "s/'[^']*'/'X'/g" | sort | uniq -c | sort -rn | head -15
print ""
python3 "${RA_ROOT}/port/check-dup-headers.py" | tail -1
print ""
print "logs: $OUT"
