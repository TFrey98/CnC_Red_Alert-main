#!/bin/zsh
#
# run.sh -- regression tests for the engine's data-path primitives.
#
# These exist because the most damaging bugs in this port do not show up as
# compile errors: `long` is 64 bits on macOS and was 32 on Win32, which silently
# broke the CRC that indexes every MIX archive, the SHA-1 that verifies them, and
# the RSA arithmetic that decrypts their headers. Each test compiles the REAL
# engine source for arm64 and checks it against an independent reference.
#
# Run after touching CRC.*, SHA.*, MP.*, INT.* or anything that changes `long`.

set -e
RA_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
T="${RA_ROOT}/port/tests"
OUT="${TMPDIR:-/tmp}/ra-tests"; mkdir -p "$OUT"
build() { clang++ $RA_CXXFLAGS -w -g -fsanitize=address -iquote "${RA_ROOT}/CODE" "$@"; }
fail=0
run() { print -n "$1: "; shift; if "$@"; then :; else fail=1; fi; }

build "$T/crc_vs_asm.cpp"   "${RA_ROOT}/CODE/CRC.CPP" -o "$OUT/crc_vs_asm"
build "$T/crc_chunked.cpp"  "${RA_ROOT}/CODE/CRC.CPP" -o "$OUT/crc_chunked"
build "$T/sha1_fips.cpp"    "${RA_ROOT}/CODE/SHA.CPP" -o "$OUT/sha1_fips"
build "$T/sha1_million_a.cpp" "${RA_ROOT}/CODE/SHA.CPP" -o "$OUT/sha1_million_a"
build "$T/rsa_modexp.cpp"   "${RA_ROOT}/CODE/MP.CPP" "${RA_ROOT}/CODE/INT.CPP" -o "$OUT/rsa_modexp"

print "CRC (MIX filename hash) vs a line-by-line model of WIN32LIB/MISC/CRC.ASM"
"$OUT/crc_vs_asm" | tail -1 || fail=1
"$OUT/crc_chunked" || fail=1
print "SHA-1 (MIX digest) vs FIPS 180 vectors"
"$OUT/sha1_fips" || fail=1
"$OUT/sha1_million_a" || fail=1
print "RSA modular exponentiation vs Python pow(), exponent < modulus"
python3 -c "
import sys
rows=[l.split() for l in open('$T/rsa_modexp_vectors.txt')]
open('$OUT/rsa_valid.txt','w').write(''.join(' '.join(r)+'\n' for r in rows if int(r[1],16) < int(r[2],16)))"
KAT="$OUT/rsa_valid.txt" "$OUT/rsa_modexp" || fail=1

print "Coordinate composition (XY_Coord) at -O0, where a 64-bit COORDINATE leaked stack bytes"
clang++ $RA_CXXFLAGS -w -O0 -iquote "${RA_ROOT}/CODE" -c "$T/coord_compose.cpp" -o "$OUT/coord_compose.o"
clang++ $RA_TARGET "$OUT/coord_compose.o" -o "$OUT/coord_compose"
"$OUT/coord_compose" | tail -1 | grep -q "exact" && "$OUT/coord_compose" | tail -1 || { "$OUT/coord_compose"; fail=1 }

print "DOS directory search (scenario archives, save-game list)"
clang++ $RA_CXXFLAGS -w -g -fsanitize=address "$T/dos_find.cpp" "${RA_ROOT}/port/compat/wwcompat.cpp" -o "$OUT/dos_find"
"$OUT/dos_find" | tail -1 | grep -q "all pass" && "$OUT/dos_find" | tail -1 || { "$OUT/dos_find"; fail=1 }

print "Win32 file API on POSIX (RawFileClass: every MIX, INI and save file)"
clang++ $RA_CXXFLAGS -w -g -fsanitize=address "$T/win32_file.cpp" "${RA_ROOT}/port/compat/wwcompat.cpp" -o "$OUT/win32_file"
"$OUT/win32_file" | tail -1 | grep -q "all pass" && "$OUT/win32_file" | tail -1 || { "$OUT/win32_file"; fail=1 }

print "RawFileClass (CODE/RAWFILE.CPP) end to end, incl. MIX-style Bias windows"
clang++ $RA_CXXFLAGS -w -g -fsanitize=address -iquote "${RA_ROOT}/CODE" "$T/rawfile.cpp" "${RA_ROOT}/CODE/RAWFILE.CPP" "${RA_ROOT}/port/compat/wwcompat.cpp" -o "$OUT/rawfile"
"$OUT/rawfile" | tail -1 | grep -q "all pass" && "$OUT/rawfile" | tail -1 || { "$OUT/rawfile"; fail=1 }

print ""
(( fail )) && { print "FAILED"; exit 1 } || print "all data-path tests pass"
