#!/bin/zsh
# probe-one.sh <outdir> <file> -- syntax-check a single TU; used by probe.sh.
RA_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
OUT="$1"; f="$2"
cd "${RA_ROOT}/CODE"
if clang++ $RA_CXXFLAGS -fsyntax-only -w "$f" 2>"${OUT}/${f}.log"; then
  rm -f "${OUT}/${f}.log"; print "OK $f"
else
  print "FAIL $f"
fi
