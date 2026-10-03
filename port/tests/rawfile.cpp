/*
** rawfile.cpp -- the engine's real RawFileClass (CODE/RAWFILE.CPP) over the
** POSIX-backed Win32 file API. Bias() is how MIX archives expose each embedded
** file as a window onto the archive, so it is tested explicitly.
*/
#include "rawfile.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
static int fails = 0;
static void check(bool ok, const char * what) { printf("  %-62s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) fails++; }
int main() {
	char dir[] = "/tmp/ra_rawfile_XXXXXX"; mkdtemp(dir); chdir(dir);
	char data[3000]; for (int i = 0; i < 3000; i++) data[i] = (char)('A' + i % 26);
	{
		RawFileClass f("ARCHIVE.MIX");
		check(f.Open(WRITE) != 0, "Open(WRITE)");
		check(f.Write(data, 3000) == 3000, "Write 3000 bytes");
		f.Close();
	}
	RawFileClass f("ARCHIVE.MIX");
	check(f.Is_Available() != 0, "Is_Available");
	check(f.Size() == 3000, "Size");
	char buf[3000];
	check(f.Open(READ) != 0 && f.Read(buf, 3000) == 3000 && memcmp(buf, data, 3000) == 0, "Open(READ) + Read round-trips the data");
	check(f.Seek(1000, SEEK_SET) == 1000 && f.Read(buf, 5) == 5 && memcmp(buf, data + 1000, 5) == 0, "Seek + Read");
	check(f.Read(buf, 5000) == 1995, "read past EOF returns the true remainder and terminates");
	f.Close();

	/* a 100-byte window starting at offset 500, as MixFileClass presents an embedded file */
	RawFileClass sub("ARCHIVE.MIX");
	sub.Bias(500, 100);
	check(sub.Size() == 100, "Bias(500,100): Size is the window, not the file");
	check(sub.Open(READ) != 0 && sub.Read(buf, 1000) == 100 && memcmp(buf, data + 500, 100) == 0, "Bias: Read is clipped to the window and offset by its start");
	check(sub.Seek(0, SEEK_SET) == 0 && sub.Seek(10, SEEK_CUR) == 10 && sub.Read(buf, 1) == 1 && buf[0] == data[510], "Bias: seeks are relative to the window");
	sub.Close();

	RawFileClass missing("NOTHERE.INI");
	check(missing.Is_Available() == 0, "missing file is not available");
	check(f.Delete() != 0 && access("ARCHIVE.MIX", F_OK) != 0, "Delete");
	char cmd[1100]; snprintf(cmd, sizeof cmd, "rm -rf %s", dir); system(cmd);
	printf("%s\n", fails ? "FAILED" : "rawfile: all pass");
	return fails != 0;
}
