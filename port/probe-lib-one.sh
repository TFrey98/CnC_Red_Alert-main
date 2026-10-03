#!/bin/zsh
# probe-lib-one.sh <outdir> <LIB/MODULE/FILE> -- syntax-check one library TU
# from its own directory, with its library's flags. Used by probe-libs.sh.
RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
OUT="$1"; rel="$2"; log="${OUT}/${rel//\//_}.log"
case "$rel" in
  WINVQ/*) flags=($RA_WINVQ_CXXFLAGS) ;;
  *)       flags=($RA_WIN32LIB_CXXFLAGS) ;;
esac
lang=(); [[ "$rel" == *.C || "$rel" == *.c ]] && lang=(-x c++)
cd "${RA_ROOT}/$(dirname "$rel")"
if clang++ $flags $lang -fsyntax-only -w "$(basename "$rel")" 2>"$log"; then
  rm -f "$log"; print "OK $rel"
else
  print "FAIL $rel"
fi
