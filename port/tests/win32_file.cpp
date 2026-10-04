/*
** win32_file.cpp -- the POSIX implementation of the Win32 file API that
** RawFileClass (and so every MIX, INI and save file) is built on.
*/
#include "windows.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
static int fails = 0;
static void check(bool ok, const char * what) { printf("  %-64s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) fails++; }
int main() {
	char dir[] = "/tmp/ra_w32file_XXXXXX"; mkdtemp(dir); chdir(dir);
	unsigned char data[5000]; for (int i = 0; i < 5000; i++) data[i] = (unsigned char)(i * 7 + 3);

	HANDLE w = CreateFile("SAVEGAME.000", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	check(w != INVALID_HANDLE_VALUE, "CreateFile CREATE_ALWAYS");
	DWORD n = 0; check(WriteFile(w, data, sizeof data, &n, NULL) && n == sizeof data, "WriteFile writes all 5000 bytes");
	check(CloseHandle(w), "CloseHandle on a file");

	HANDLE r = CreateFile("SAVEGAME.000", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	check(r != INVALID_HANDLE_VALUE, "CreateFile OPEN_EXISTING");
	check(GetFileSize(r, NULL) == 5000, "GetFileSize");
	unsigned char back[5000]; n = 0;
	check(ReadFile(r, back, 5000, &n, NULL) && n == 5000 && memcmp(back, data, 5000) == 0, "ReadFile round-trips the data exactly");
	check(SetFilePointer(r, 100, NULL, FILE_BEGIN) == 100, "SetFilePointer FILE_BEGIN");
	check(SetFilePointer(r, 50, NULL, FILE_CURRENT) == 150, "SetFilePointer FILE_CURRENT");
	check(SetFilePointer(r, -10, NULL, FILE_END) == 4990, "SetFilePointer FILE_END, negative");
	n = 123; check(ReadFile(r, back, 100, &n, NULL) && n == 10, "short read at EOF reports the true count (10), returns TRUE");
	n = 123; check(ReadFile(r, back, 100, &n, NULL) && n == 0, "read at EOF returns TRUE with 0 bytes, like Win32");
	CloseHandle(r);

	check(CreateFile("NOPE.INI", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "missing file: INVALID_HANDLE_VALUE + ERROR_FILE_NOT_FOUND");
	check(CreateFile("SAVEGAME.000", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS, "CREATE_NEW on existing file fails with ERROR_FILE_EXISTS");

	mkdir("DATA", 0755);
	HANDLE b = CreateFile("DATA\\RULES.INI", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	check(b != INVALID_HANDLE_VALUE && access("DATA/RULES.INI", F_OK) == 0, "DOS backslash path maps to a POSIX directory");
	CloseHandle(b);
	HANDLE ci = CreateFile("data\\rules.ini", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	check(ci != INVALID_HANDLE_VALUE, "case-insensitive open (this volume's default; see note in PORTING.md)");
	CloseHandle(ci);

	/* tagging: a closed descriptor must really be closed, and non-file handles must be left alone */
	HANDLE t = CreateFile("SAVEGAME.000", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	int fd = (int)((intptr_t)t & 0x00FFFFFF);
	CloseHandle(t);
	check(fcntl(fd, F_GETFD) == -1 && errno == EBADF, "CloseHandle really closes the descriptor (no leak)");
	int keep = open("SAVEGAME.000", O_RDONLY);
	CloseHandle((HANDLE)(intptr_t)keep);			/* looks like an untagged event/mapping handle */
	check(fcntl(keep, F_GETFD) != -1, "CloseHandle leaves untagged (non-file) handles alone");
	close(keep);
	check(CloseHandle(INVALID_HANDLE_VALUE) == TRUE, "CloseHandle(INVALID_HANDLE_VALUE) is harmless");

	/* FILETIME epoch and DOS date/time */
	struct timespec epoch = {0, 0}; FILETIME ft;
	struct tm z = {}; z.tm_year = 70; z.tm_mday = 1;
	WORD dd, dt; FILETIME f2;
	check(DosDateTimeToFileTime((WORD)((17 << 9) | (11 << 5) | 21), (WORD)((13 << 11) | (45 << 5) | 15), &f2), "DosDateTimeToFileTime");
	check(FileTimeToDosDateTime(&f2, &dd, &dt) && dd == ((17 << 9) | (11 << 5) | 21) && dt == ((13 << 11) | (45 << 5) | 15), "DOS date/time round-trips exactly (1997-11-21 13:45:30)");
	HANDLE s = CreateFile("SAVEGAME.000", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	BY_HANDLE_FILE_INFORMATION info;
	check(SetFileTime(s, NULL, &f2, &f2) && GetFileInformationByHandle(s, &info), "SetFileTime + GetFileInformationByHandle");
	check(FileTimeToDosDateTime(&info.ftLastWriteTime, &dd, &dt) && dd == ((17 << 9) | (11 << 5) | 21) && dt == ((13 << 11) | (45 << 5) | 15), "timestamp survives a round trip through the filesystem");
	check(info.nFileSizeLow == 5000, "file information reports the size");
	CloseHandle(s);
	(void)epoch; (void)ft; (void)z;

	/* FindFirstFile: SESSION.CPP lists *.PKT mission packs and *.MPR user maps */
	{ FILE * f; f = fopen("ALLIED.MPR", "w"); fclose(f); f = fopen("soviet.mpr", "w"); fclose(f); f = fopen("NOTAMAP.TXT", "w"); fclose(f); mkdir("DIR.MPR", 0755); }
	WIN32_FIND_DATA fdata; int found = 0, dirs = 0;
	HANDLE fh = FindFirstFile("*.MPR", &fdata);
	if (fh != INVALID_HANDLE_VALUE) {
		do {
			if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirs++; else found++;
		} while (FindNextFile(fh, &fdata));
		FindClose(fh);
	}
	check(found == 2 && dirs == 1 && fdata.cAlternateFileName[0] == 0, "FindFirstFile *.MPR: case-insensitive, directories flagged, 8.3 name empty");
	check(FindFirstFile("*.PKT", &fdata) == INVALID_HANDLE_VALUE, "FindFirstFile with no match returns INVALID_HANDLE_VALUE");
	check(DeleteFile("SAVEGAME.000") && access("SAVEGAME.000", F_OK) != 0, "DeleteFile");
	char cmd[1100]; snprintf(cmd, sizeof cmd, "rm -rf %s", dir); system(cmd);
	printf("%s\n", fails ? "FAILED" : "win32_file: all pass");
	return fails != 0;
}
