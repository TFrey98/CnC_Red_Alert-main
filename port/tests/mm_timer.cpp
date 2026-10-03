/*
** mm_timer.cpp -- Win32 multimedia timers on GCD. These drive the game clock
** (TIMERINI.CPP), the mouse, sound maintenance and the VQA player.
*/
#include "mmsystem.h"
#include <stdio.h>
#include <unistd.h>
#include <atomic>
static int fails = 0;
static void check(bool ok, const char * what) { printf("  %-66s %s\n", what, ok ? "ok" : "FAIL"); if (!ok) fails++; }

static std::atomic<int> ticks(0), oneshot(0), inside(0), overlaps(0), slow_done(0), self_killed(0);
static UINT self_id = 0;
static void CALLBACK on_tick(UINT, UINT, DWORD, DWORD, DWORD) { ticks++; }
static void CALLBACK on_once(UINT, UINT, DWORD, DWORD, DWORD) { oneshot++; }
static void CALLBACK guarded(UINT, UINT, DWORD, DWORD, DWORD) {
	if (inside.exchange(1)) overlaps++;
	usleep(2000);
	inside = 0;
}
static void CALLBACK slow(UINT, UINT, DWORD, DWORD, DWORD) { usleep(60000); slow_done = 1; }
static void CALLBACK kills_itself(UINT id, UINT, DWORD, DWORD, DWORD) { if (timeKillEvent(id) == TIMERR_NOERROR) self_killed++; }

int main() {
	UINT t = timeSetEvent(1000/60, 1, on_tick, 0, TIME_PERIODIC);
	usleep(1000000);
	timeKillEvent(t);
	int n = ticks; check(n >= 54 && n <= 66, "60 Hz periodic timer: ~60 callbacks in one second (the game clock rate)");
	usleep(100000);
	check(ticks == n, "no callbacks after timeKillEvent");

	timeSetEvent(20, 1, on_once, 0, TIME_ONESHOT);
	usleep(150000);
	check(oneshot == 1, "TIME_ONESHOT fires exactly once");

	UINT a = timeSetEvent(3, 1, guarded, 0, TIME_PERIODIC), b = timeSetEvent(4, 1, guarded, 0, TIME_PERIODIC);
	usleep(300000);
	timeKillEvent(a); timeKillEvent(b);
	check(overlaps == 0, "callbacks of different timers never overlap (Win32's single timer thread)");

	UINT s = timeSetEvent(5, 1, slow, 0, TIME_PERIODIC);
	usleep(20000);						/* let a slow callback start */
	timeKillEvent(s);
	check(slow_done == 1, "timeKillEvent waits for an in-flight callback before returning");

	self_id = timeSetEvent(5, 1, kills_itself, 0, TIME_PERIODIC);
	usleep(100000);
	check(self_killed == 1, "a callback can kill its own timer without deadlocking");

	DWORD t0 = timeGetTime(); usleep(50000); DWORD t1 = timeGetTime();
	check(t1 - t0 >= 45 && t1 - t0 <= 80, "timeGetTime advances in milliseconds");
	check(timeKillEvent(0) == TIMERR_NOCANDO && timeKillEvent(123456) == TIMERR_NOCANDO, "killing an unknown timer fails cleanly");
	printf("%s\n", fails ? "FAILED" : "mm_timer: all pass");
	return fails != 0;
}
