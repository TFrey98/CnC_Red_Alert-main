#!/bin/zsh
#
# show-disk-error-dialog.sh -- MANUAL check: shows the real native dialog the
# game raises on a disk read error, and prints which button you chose.
# Not part of run.sh, because it puts a window on screen and waits for a click.

set -e
RA_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "${RA_ROOT}/port/flags.sh"
OUT="${TMPDIR:-/tmp}/ra-dialog-demo"; mkdir -p "$OUT"
cat > "$OUT/demo.cpp" <<'CPP'
#include "ra_platform.h"
#include <stdio.h>
int main() {
	int choice = RA_Platform_Disk_Error("MAIN.MIX", 5);
	printf("You chose: %s\n", choice == RA_DISK_ERROR_RETRY ? "Try Again" : "Cancel");
	return 0;
}
CPP
clang++ $RA_CXXFLAGS -w -c "$OUT/demo.cpp" -o "$OUT/demo.o"
clang++ $RA_OBJCXXFLAGS -c "${RA_ROOT}/port/backend/ra_dialog.mm" -o "$OUT/ra_dialog.o"
clang++ $RA_TARGET "$OUT/demo.o" "$OUT/ra_dialog.o" -o "$OUT/demo" $RA_FRAMEWORKS
"$OUT/demo"
