#!/bin/zsh
#
# layout-check.sh -- find structs whose layout differs between translation
# units: the same type, compiled under different #pragma pack state (or other
# include-order effects), getting different sizes or field offsets. Each unit
# then reads and writes the "same" object at different places, silently.
#
# Why this exists: several Westwood headers set a bare `#pragma pack(n)` and
# never restore it, so the packing of every struct declared after them depends
# on which headers a file happened to include first. On Watcom's 32-bit build
# that was harmless -- no field was wider than 4 bytes, so pack(4) and the
# default agreed. With 8-byte pointers they don't. This is how
# LockedData.SoundVolume read as 0 in SOUNDIO.CPP while SOUNDLCK.CPP had set it
# to 255: the two files put it 104 bytes apart.
#
# Reads the unit lists written by port/link-census.sh (run that first).
# Usage: port/layout-check.sh [--all]   (--all also lists same-named types
#                                        that only differ in name-clash cases)

RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
L="${TMPDIR:-/tmp}/ra-link"
[[ -f "$L/game.txt" && -f "$L/lib.txt" ]] || { echo "run port/link-census.sh first"; exit 1; }
O="${TMPDIR:-/tmp}/ra-layout"; rm -rf "$O"; mkdir -p "$O"

cat > "$O/dump.sh" <<'DUMP'
#!/bin/zsh
source "$RA_ROOT/port/flags.sh"
O="$1"; kind="$2"; rel="$3"
if [[ $kind == game ]]; then
  cd "$RA_ROOT/CODE"; flags=($RA_CXXFLAGS); src="$rel"; tag="CODE/$rel"
elif [[ $kind == port ]]; then
  cd "$RA_ROOT"; flags=($RA_CXXFLAGS -I port/backend); src="$rel"; tag="$rel"
else
  case "$rel" in WINVQ/*) flags=($RA_WINVQ_CXXFLAGS) ;; *) flags=($RA_WIN32LIB_CXXFLAGS) ;; esac
  [[ "$rel" == *.C || "$rel" == *.c ]] && flags+=(-x c++)
  cd "$RA_ROOT/$(dirname $rel)"; src="$(basename $rel)"; tag="$rel"
fi
clang++ $flags -w -fsyntax-only -Xclang -fdump-record-layouts-simple "$src" > "$O/${tag//\//_}.layout" 2>/dev/null
echo "$tag" > "$O/${tag//\//_}.tag"
DUMP
chmod +x "$O/dump.sh"
export RA_ROOT
cd "$RA_ROOT"
xargs -P 8 -I{} "$O/dump.sh" "$O" game {} < "$L/game.txt"
xargs -P 8 -I{} "$O/dump.sh" "$O" lib  {} < "$L/lib.txt"
for f in port/compat/wwcompat.cpp port/compat/win32_*.cpp; do "$O/dump.sh" "$O" port "$f"; done

python3 - "$O" "$1" <<'PY'
import glob, os, re, sys
from collections import defaultdict
O, show_all = sys.argv[1], (len(sys.argv) > 2 and sys.argv[2] == '--all')
seen = defaultdict(lambda: defaultdict(list))   # type -> layout -> [units]
for path in glob.glob(os.path.join(O, '*.layout')):
    unit = open(path[:-7] + '.tag').read().strip()
    text = open(path, errors='replace').read()
    for block in re.split(r'\n(?=\*\*\* Dumping AST Record Layout)', text):
        m = re.search(r'Type: (.+)\n', block)
        if not m: continue
        name = m.group(1).strip()
        size = re.search(r'Size:(\d+)', block)
        offs = re.search(r'FieldOffsets: \[([^\]]*)\]', block)
        if not size: continue
        layout = (int(size.group(1)) // 8, offs.group(1) if offs else '')
        if unit not in seen[name][layout]:
            seen[name][layout].append(unit)
bad = {n: v for n, v in seen.items() if len(v) > 1}
# A type whose name carries its defining location ("at FILE:LINE") is one
# definition; a bare name may be two different types that share a name.
exact = {n: v for n, v in bad.items() if ' at ' in n}
named = {n: v for n, v in bad.items() if ' at ' not in n}
print(f'{len(seen)} record types; {len(exact)} unnamed and {len(named)} named types with differing layouts')
for title, group in (('UNNAMED (one definition, so certainly a mismatch)', exact), ('NAMED (check: same definition, or a name clash?)', named)):
    if not group: continue
    print('\n== ' + title)
    for n in sorted(group):
        print(f'\n{n}')
        for (sz, offs), units in sorted(group[n].items()):
            shown = ', '.join(sorted(units)[:4]) + (f' +{len(units)-4} more' if len(units) > 4 else '')
            print(f'   size {sz:5d}  offsets(bits) [{offs[:70]}{"..." if len(offs) > 70 else ""}]  <- {shown}')
PY
