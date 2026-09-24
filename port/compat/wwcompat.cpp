/*
**	wwcompat.cpp -- implementations of the non-standard CRT entry points
**	declared in wwcompat.h.
*/
#include "wwcompat.h"
#include <unistd.h>
#include <sys/stat.h>

extern "C" {

/*
**	Shared worker for itoa/ltoa/ultoa. Radix 10 with a negative value is the
**	only case that emits a sign; every other radix formats the raw bit pattern,
**	matching the Watcom behaviour the game depends on.
*/
static char * wwport_int_to_string(unsigned long value, char * buffer, int radix, int negative)
{
	static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	char   temp[8 * sizeof(unsigned long) + 2];
	int    len = 0;
	char * out = buffer;

	if (buffer == 0 || radix < 2 || radix > 36) return buffer;

	if (value == 0) {
		temp[len++] = '0';
	} else {
		while (value != 0) {
			temp[len++] = digits[value % (unsigned long)radix];
			value /= (unsigned long)radix;
		}
	}

	if (negative) *out++ = '-';
	while (len > 0) *out++ = temp[--len];
	*out = '\0';
	return buffer;
}

char * itoa(int value, char * buffer, int radix)
{
	if (radix == 10 && value < 0) {
		/* Negating INT_MIN overflows, so widen before negating. */
		return wwport_int_to_string((unsigned long)(-(long)value), buffer, radix, 1);
	}
	return wwport_int_to_string((unsigned long)(unsigned int)value, buffer, radix, 0);
}

char * ltoa(long value, char * buffer, int radix)
{
	if (radix == 10 && value < 0) {
		return wwport_int_to_string(0UL - (unsigned long)value, buffer, radix, 1);
	}
	return wwport_int_to_string((unsigned long)value, buffer, radix, 0);
}

char * ultoa(unsigned long value, char * buffer, int radix)
{
	return wwport_int_to_string(value, buffer, radix, 0);
}

char * strupr(char * string)
{
	if (string != 0) {
		for (char * p = string; *p != '\0'; p++) *p = (char)toupper((unsigned char)*p);
	}
	return string;
}

char * strlwr(char * string)
{
	if (string != 0) {
		for (char * p = string; *p != '\0'; p++) *p = (char)tolower((unsigned char)*p);
	}
	return string;
}

char * strrev(char * string)
{
	if (string != 0) {
		size_t len = strlen(string);
		for (size_t i = 0; i + 1 < len - i; i++) {
			char swap = string[i];
			string[i] = string[len - 1 - i];
			string[len - 1 - i] = swap;
		}
	}
	return string;
}

int wwport_memicmp(const void * a, const void * b, size_t count)
{
	const unsigned char * pa = (const unsigned char *)a;
	const unsigned char * pb = (const unsigned char *)b;

	for (size_t i = 0; i < count; i++) {
		int ca = tolower(pa[i]);
		int cb = tolower(pb[i]);
		if (ca != cb) return ca - cb;
	}
	return 0;
}

/*
**	macOS has no drive letters, so `drive` always comes back empty. The game
**	uses these only to swap a file's extension or strip its directory, both of
**	which still work correctly under that simplification.
*/
void _splitpath(const char * path, char * drive, char * dir, char * fname, char * ext)
{
	if (drive) drive[0] = '\0';
	if (dir)   dir[0]   = '\0';
	if (fname) fname[0] = '\0';
	if (ext)   ext[0]   = '\0';
	if (path == 0) return;

	const char * slash = strrchr(path, '/');
	const char * name  = slash ? slash + 1 : path;

	if (dir && slash) {
		size_t n = (size_t)(slash - path) + 1;
		memcpy(dir, path, n);
		dir[n] = '\0';
	}

	/* A leading dot is part of the name, not an extension separator. */
	const char * dot = strrchr(name, '.');
	if (dot == name) dot = 0;

	if (fname) {
		size_t n = dot ? (size_t)(dot - name) : strlen(name);
		memcpy(fname, name, n);
		fname[n] = '\0';
	}
	if (ext && dot) {
		strcpy(ext, dot);
	}
}

void _makepath(char * path, const char * drive, const char * dir, const char * fname, const char * ext)
{
	if (path == 0) return;
	path[0] = '\0';
	(void)drive;    /* No drive letters on macOS. */

	if (dir && dir[0] != '\0') {
		strcpy(path, dir);
		size_t n = strlen(path);
		if (n > 0 && path[n - 1] != '/') {
			path[n] = '/';
			path[n + 1] = '\0';
		}
	}
	if (fname && fname[0] != '\0') strcat(path, fname);
	if (ext && ext[0] != '\0') {
		if (ext[0] != '.') strcat(path, ".");
		strcat(path, ext);
	}
}

long filelength(int handle)
{
	struct stat st;
	if (fstat(handle, &st) != 0) return -1L;
	return (long)st.st_size;
}

} /* extern "C" */

/*
**	Win32 entry points used directly by the engine. Declared in
**	port/compat/windows.h.
*/
#include "windows.h"
#include <pthread.h>
#include <time.h>

extern "C" {

void InitializeCriticalSection(LPCRITICAL_SECTION section)
{
	if (section == 0) return;
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	/* Win32 critical sections are recursive; the default pthread mutex is not. */
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&section->mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	section->initialized = 1;
}

void DeleteCriticalSection(LPCRITICAL_SECTION section)
{
	if (section == 0 || !section->initialized) return;
	pthread_mutex_destroy(&section->mutex);
	section->initialized = 0;
}

void EnterCriticalSection(LPCRITICAL_SECTION section)
{
	if (section == 0) return;
	/* Some sections are declared as globals and entered before any explicit
	** init call, which Win32 tolerated only because they were zero-filled. */
	if (!section->initialized) InitializeCriticalSection(section);
	pthread_mutex_lock(&section->mutex);
}

void LeaveCriticalSection(LPCRITICAL_SECTION section)
{
	if (section == 0 || !section->initialized) return;
	pthread_mutex_unlock(&section->mutex);
}

DWORD GetTickCount(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (DWORD)((ts.tv_sec * 1000ULL) + (ts.tv_nsec / 1000000ULL));
}

void Sleep(DWORD milliseconds)
{
	struct timespec ts;
	ts.tv_sec  = (time_t)(milliseconds / 1000);
	ts.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
	nanosleep(&ts, 0);
}

static DWORD wwport_last_error = 0;
DWORD GetLastError(void)             { return wwport_last_error; }
void  SetLastError(DWORD error)      { wwport_last_error = error; }

BOOL CloseHandle(HANDLE object)      { (void)object; return TRUE; }

void OutputDebugStringA(LPCSTR text)
{
	if (text != 0) fputs(text, stderr);
}

/*
**	Placeholder until the SDL2 layer lands -- routed to stderr so that startup
**	failures are visible rather than silent.
*/
int MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type)
{
	(void)owner; (void)type;
	fprintf(stderr, "[%s] %s\n", caption ? caption : "Red Alert", text ? text : "");
	return 1;   /* IDOK */
}

/* Keyboard state is owned by the SDL2 input layer; stubbed until it lands. */
short GetAsyncKeyState(int key) { (void)key; return 0; }
short GetKeyState(int key)      { (void)key; return 0; }

/*
**	Flat address space: a "global handle" is just the pointer itself.
*/
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes)
{
	void * mem = malloc(bytes ? bytes : 1);
	if (mem != 0 && (flags & GMEM_ZEROINIT) != 0) memset(mem, 0, bytes);
	return (HGLOBAL)mem;
}

LPVOID GlobalLock(HGLOBAL mem)   { return (LPVOID)mem; }
BOOL   GlobalUnlock(HGLOBAL mem) { (void)mem; return FALSE; }

HGLOBAL GlobalFree(HGLOBAL mem)
{
	free(mem);
	return 0;
}

} /* extern "C" */
