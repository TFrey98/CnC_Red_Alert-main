#!/bin/zsh
#
# link-census.sh -- compile every translation unit that compiles (game and
# libraries), link them once, and classify every undefined or duplicate symbol
# by cause. The successor to probe.sh once compiling is done: compiling proves
# each file is valid alone; linking proves every function the game calls exists.
#
# Libraries are linked as static archives so that, as with the original Watcom
# build, an archive member is pulled in only if something needs it.
#
# Usage: port/link-census.sh   (run port/probe.sh and probe-libs.sh first, then
#                               port/worklist.py, which this reads)

RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
L="${TMPDIR:-/tmp}/ra-link"
rm -rf "$L"; mkdir -p "$L/game" "$L/lib"
python3 - "$L" <<'PY'
import json, sys
L = sys.argv[1]
w = json.load(open('port/worklist.json'))
open(L + '/game.txt', 'w').write('\n'.join(r['file'][5:] for r in w['code'] if r['tag'] == 'DONE') + '\n')
open(L + '/lib.txt', 'w').write('\n'.join(r['file'] for r in w['lib'] if r['tag'] == 'DONE') + '\n')
PY
cat > "$L/cc.sh" <<'CC'
#!/bin/zsh
source "$RA_ROOT/port/flags.sh"
L="$1"; kind="$2"; rel="$3"
if [[ $kind == game ]]; then
  cd "$RA_ROOT/CODE"; out="$L/game/${rel%.*}.o"
  clang++ $RA_CXXFLAGS -w -c "$rel" -o "$out" 2>"$out.log" || echo "COMPILEFAIL $rel"
else
  case "$rel" in WINVQ/*) flags=($RA_WINVQ_CXXFLAGS) ;; *) flags=($RA_WIN32LIB_CXXFLAGS) ;; esac
  lang=(); [[ "$rel" == *.C || "$rel" == *.c ]] && lang=(-x c++)
  cd "$RA_ROOT/$(dirname $rel)"; out="$L/lib/${rel//\//_}"; out="${out%.*}.o"
  clang++ $flags $lang -w -c "$(basename $rel)" -o "$out" 2>"$out.log" || echo "COMPILEFAIL $rel"
fi
CC
chmod +x "$L/cc.sh"
export RA_ROOT
cd "$RA_ROOT"
xargs -P 8 -I{} "$L/cc.sh" "$L" game {} < "$L/game.txt"
xargs -P 8 -I{} "$L/cc.sh" "$L" lib  {} < "$L/lib.txt"
ar rcs "$L/libwin32lib.a" "$L"/lib/WIN32LIB_*.o
ar rcs "$L/libwinvq.a"    "$L"/lib/WINVQ_*.o
clang++ $RA_CXXFLAGS -w -c port/compat/wwcompat.cpp -o "$L/wwcompat.o"
clang++ $RA_OBJCXXFLAGS -c port/backend/ra_metal.mm  -o "$L/ra_metal.o"
clang++ $RA_OBJCXXFLAGS -c port/backend/ra_dialog.mm -o "$L/ra_dialog.o"
clang++ $RA_TARGET "$L"/game/*.o "$L/wwcompat.o" "$L/ra_metal.o" "$L/ra_dialog.o" \
        "$L/libwin32lib.a" "$L/libwinvq.a" $RA_FRAMEWORKS -o "$L/redalert" > "$L/link.txt" 2>&1
python3 "$RA_ROOT/port/link-census.py" "$L"
