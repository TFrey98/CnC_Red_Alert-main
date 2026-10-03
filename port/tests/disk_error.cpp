/*
** disk_error.cpp -- the Try Again / Cancel contract on RawFileClass reads.
**
** Each scenario runs in a child process, because Cancel quits the game.
*/
#include "rawfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
static int fails = 0;
static void check(bool ok, const char * what) { printf("  %-66s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) fails++; }
static int count_from(const char * f) { int n = 0; FILE * fp = fopen(f, "r"); if (fp) { fscanf(fp, "%d", &n); fclose(fp); } return n; }

/* Runs `body` in a child with the given scripted answers; returns its exit status (or -1 if it crashed). */
static int run(const char * answers, const char * countfile, void (*body)(void)) {
	unlink(countfile);
	fflush(stdout);		/* the Cancel path quits via exit(), which would re-flush an inherited buffer */
	pid_t pid = fork();
	if (pid == 0) { setenv("RA_TEST_ANSWERS", answers, 1); setenv("RA_TEST_COUNT_FILE", countfile, 1); body(); _exit(0); }
	int status = 0; waitpid(pid, &status, 0);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
static void open_missing(void) { RawFileClass f("NOTHERE.MIX"); if (f.Is_Available()) _exit(9); f.Open(READ); }
static void read_failing(void) {
	/* A directory opens, but read() fails with EISDIR: a genuine read error. */
	RawFileClass f("ADIR"); char buf[16];
	if (!f.Open(READ)) _exit(8);
	f.Read(buf, sizeof buf);
	_exit(7);	/* only reached if the read gave up without quitting */
}
int main() {
	char dir[] = "/tmp/ra_diskerr_XXXXXX"; mkdtemp(dir); chdir(dir); mkdir("ADIR", 0755);
	char countfile[1100]; snprintf(countfile, sizeof countfile, "%s/count", dir);

	int st = run("", countfile, open_missing);
	check(st == 0 && count_from(countfile) == 0, "a missing file at open time asks nothing (Is_Available stays silent)");

	st = run("C", countfile, read_failing);
	check(st == EXIT_FAILURE && count_from(countfile) == 1, "read error + Cancel: asked once, game quits");

	st = run("RRC", countfile, read_failing);
	check(st == EXIT_FAILURE && count_from(countfile) == 3, "Try Again retries the read: asked again each time, until Cancel");

	char cmd[1200]; snprintf(cmd, sizeof cmd, "rm -rf %s", dir); system(cmd);
	printf("%s\n", fails ? "FAILED" : "disk_error: all pass");
	return fails != 0;
}
