/*
** dos_find.cpp -- _dos_findfirst/_dos_findnext against the two patterns the
** engine uses: "SC*.MIX" (INIT.CPP registers scenario archives) and
** "SAVEGAME.*" (LOADDLG.CPP lists save slots and sorts by DOS date/time).
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
static int fails = 0;
static void check(bool ok, const char * what) { printf("  %-62s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) fails++; }
static void touch(const char * dir, const char * name, time_t when) {
	char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, name);
	FILE * f = fopen(p, "w"); fputs("x", f); fclose(f);
	struct timeval tv[2] = {{when, 0}, {when, 0}}; utimes(p, tv);
}
static std::set<std::string> find_all(const char * pattern, struct find_t * last) {
	std::set<std::string> out; struct find_t ff;
	if (_dos_findfirst(pattern, _A_NORMAL, &ff) == 0) { do { out.insert(ff.name); *last = ff; } while (_dos_findnext(&ff) == 0); }
	return out;
}
int main() {
	char dir[] = "/tmp/ra_dosfind_XXXXXX"; mkdtemp(dir);
	struct tm t = {}; t.tm_year = 1997-1900; t.tm_mon = 10; t.tm_mday = 21; t.tm_hour = 13; t.tm_min = 45; t.tm_sec = 30; t.tm_isdst = -1;
	time_t when = mktime(&t);
	for (const char * n : {"SAVEGAME.000", "savegame.001", "SAVEGAME", "SCG01EA.MIX", "sc-new.mix", "NOTES.TXT", ".SAVEGAME.HID", "SAVEGAME.TOOLONGNAME"}) touch(dir, n, when);
	char sub[1024]; snprintf(sub, sizeof sub, "%s/SAVEGAME.DIR", dir); mkdir(sub, 0755);
	chdir(dir);
	struct find_t last;
	std::set<std::string> sc = find_all("SC*.MIX", &last);
	check(sc == std::set<std::string>({"SCG01EA.MIX", "sc-new.mix"}), "SC*.MIX finds both scenario archives, case-insensitively");
	std::set<std::string> sv = find_all("SAVEGAME.*", &last);
	check(sv.count("SAVEGAME.000") && sv.count("savegame.001"), "SAVEGAME.* finds both saves, case-insensitively");
	check(sv.count("SAVEGAME") == 1, "SAVEGAME.* also matches an extensionless name (DOS rule)");
	check(sv.count("SAVEGAME.DIR") == 0, "directories excluded without _A_SUBDIR");
	check(sv.count(".SAVEGAME.HID") == 0, "dot-files treated as hidden");
	check(sv.count("SAVEGAME.TOOLONGNAME") == 0, "names too long for the 8.3 field are skipped, not truncated");
	unsigned date = last.wr_date, time_ = last.wr_time;
	check(((date >> 9) + 1980) == 1997 && ((date >> 5) & 15) == 11 && (date & 31) == 21, "wr_date packs 1997-11-21 in DOS format");
	check((time_ >> 11) == 13 && ((time_ >> 5) & 63) == 45 && (time_ & 31) == 15, "wr_time packs 13:45:30 in DOS format (seconds/2)");
	check(last.size == 1, "size");
	struct find_t ff; check(_dos_findfirst("NOMATCH*.ZZZ", _A_NORMAL, &ff) != 0, "no match returns nonzero");
	for (int i = 0; i < 40; i++) { struct find_t g; _dos_findfirst("SC*.MIX", _A_NORMAL, &g); }   /* abandoned searches */
	check(find_all("SC*.MIX", &last).size() == 2, "still correct after 40 abandoned searches (slot reclamation)");
	char cmd[1100]; snprintf(cmd, sizeof cmd, "rm -rf %s", dir); system(cmd);
	printf("%s\n", fails ? "FAILED" : "dos_find: all pass");
	return fails != 0;
}
