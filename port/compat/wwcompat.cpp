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

/*
**	File HANDLEs from CreateFileA are tagged descriptors (see wwport_file_fd
**	below). Only those are closed; the engine also passes event and mapping
**	handles here (W95TRACE.CPP), which must never be mistaken for descriptors.
*/
int wwport_file_fd(HANDLE h);
BOOL CloseHandle(HANDLE object)
{
	int fd = wwport_file_fd(object);
	if (fd < 0) return TRUE;
	return close(fd) == 0 ? TRUE : FALSE;
}

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

/*
**	SYSTEMTIME. C++ linkage, matching the declarations in windows.h (they sit
**	outside its extern "C" block). Real implementations: MENUS.CPP seeds the
**	cryptographic RNG from wMilliseconds, so a stub returning zeros would quietly
**	weaken that seed.
*/
static void wwport_fill_systemtime(LPSYSTEMTIME out, bool utc)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	time_t secs = ts.tv_sec;
	struct tm parts;
	if (utc) gmtime_r(&secs, &parts); else localtime_r(&secs, &parts);
	out->wYear         = (WORD)(parts.tm_year + 1900);
	out->wMonth        = (WORD)(parts.tm_mon + 1);	/* Win32 months are 1-12 */
	out->wDayOfWeek    = (WORD)parts.tm_wday;		/* both 0 = Sunday */
	out->wDay          = (WORD)parts.tm_mday;
	out->wHour         = (WORD)parts.tm_hour;
	out->wMinute       = (WORD)parts.tm_min;
	out->wSecond       = (WORD)parts.tm_sec;
	out->wMilliseconds = (WORD)(ts.tv_nsec / 1000000);
}

void GetSystemTime(LPSYSTEMTIME time) { if (time) wwport_fill_systemtime(time, true); }
void GetLocalTime(LPSYSTEMTIME time)  { if (time) wwport_fill_systemtime(time, false); }


/*
**	_dos_findfirst / _dos_findnext -- see the declaration in wwcompat.h.
**
**	DOS matched wildcards case-insensitively and treated "NAME.*" as also
**	matching a name with no extension; both are reproduced. Searches are kept
**	in a small table because the engine, like DOS code generally, never closes
**	a search it has finished with; a slot is released when its search runs dry.
*/
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>

struct wwport_search {
	bool     used;
	DIR *    dir;
	char     dirpath[1024];
	char     pattern[256];
	unsigned attributes;
};
static wwport_search wwport_searches[16];
static const int wwport_search_count = (int)(sizeof(wwport_searches) / sizeof(wwport_searches[0]));

static bool wwport_dos_match(const char * pattern, const char * name)
{
	if (fnmatch(pattern, name, FNM_CASEFOLD) == 0) return true;
	size_t n = strlen(pattern);
	if (n >= 2 && pattern[n-2] == '.' && pattern[n-1] == '*' && strchr(name, '.') == NULL) {
		char stem[256];
		if (n - 2 >= sizeof(stem)) return false;
		memcpy(stem, pattern, n - 2);
		stem[n - 2] = '\0';
		return fnmatch(stem, name, FNM_CASEFOLD) == 0;
	}
	return false;
}

static unsigned wwport_find_advance(int slot, struct find_t * result)
{
	wwport_search & s = wwport_searches[slot];
	struct dirent * entry;
	while ((entry = readdir(s.dir)) != NULL) {
		const char * name = entry->d_name;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		if (strlen(name) >= sizeof(result->name)) continue;
		if (!wwport_dos_match(s.pattern, name)) continue;

		char full[1400];
		snprintf(full, sizeof(full), "%s/%s", s.dirpath, name);
		struct stat st;
		if (stat(full, &st) != 0) continue;

		unsigned attrib = 0;
		if (S_ISDIR(st.st_mode)) attrib |= _A_SUBDIR;
		if (name[0] == '.')      attrib |= _A_HIDDEN;
		if (access(full, W_OK) != 0) attrib |= _A_RDONLY;
		if ((attrib & _A_SUBDIR) && !(s.attributes & _A_SUBDIR)) continue;
		if ((attrib & _A_HIDDEN) && !(s.attributes & _A_HIDDEN)) continue;

		struct tm parts;
		time_t mtime = st.st_mtime;
		localtime_r(&mtime, &parts);
		int year = parts.tm_year + 1900 - 1980;
		if (year < 0) year = 0;
		if (year > 127) year = 127;
		result->wr_date = (unsigned short)((year << 9) | ((parts.tm_mon + 1) << 5) | parts.tm_mday);
		result->wr_time = (unsigned short)((parts.tm_hour << 11) | (parts.tm_min << 5) | (parts.tm_sec / 2));
		result->attrib  = (char)attrib;
		result->size    = (uint32_t)st.st_size;
		strcpy(result->name, name);
		memcpy(result->reserved, &slot, sizeof(slot));
		return 0;
	}
	closedir(s.dir);
	s.used = false;
	return ENOENT;
}

extern "C" {

unsigned _dos_findfirst(const char * pattern, unsigned attributes, struct find_t * result)
{
	if (pattern == NULL || result == NULL) return EINVAL;
	int slot = -1;
	for (int i = 0; i < wwport_search_count; i++) {
		if (!wwport_searches[i].used) { slot = i; break; }
	}
	if (slot < 0) {				/* every slot abandoned mid-search: reclaim the first */
		closedir(wwport_searches[0].dir);
		wwport_searches[0].used = false;
		slot = 0;
	}
	wwport_search & s = wwport_searches[slot];
	const char * slash = strrchr(pattern, '/');
	if (slash == NULL) slash = strrchr(pattern, '\\');
	if (slash != NULL) {
		size_t n = (size_t)(slash - pattern);
		if (n >= sizeof(s.dirpath)) return ENAMETOOLONG;
		memcpy(s.dirpath, pattern, n);
		s.dirpath[n] = '\0';
		if (n == 0) strcpy(s.dirpath, "/");
		snprintf(s.pattern, sizeof(s.pattern), "%s", slash + 1);
	} else {
		strcpy(s.dirpath, ".");
		snprintf(s.pattern, sizeof(s.pattern), "%s", pattern);
	}
	s.dir = opendir(s.dirpath);
	if (s.dir == NULL) return ENOENT;
	s.used = true;
	s.attributes = attributes;
	return wwport_find_advance(slot, result);
}

unsigned _dos_findnext(struct find_t * result)
{
	if (result == NULL) return EINVAL;
	int slot;
	memcpy(&slot, result->reserved, sizeof(slot));
	if (slot < 0 || slot >= wwport_search_count || !wwport_searches[slot].used) return ENOENT;
	return wwport_find_advance(slot, result);
}

unsigned _dos_findclose(struct find_t * result)
{
	if (result == NULL) return EINVAL;
	int slot;
	memcpy(&slot, result->reserved, sizeof(slot));
	if (slot >= 0 && slot < wwport_search_count && wwport_searches[slot].used) {
		closedir(wwport_searches[slot].dir);
		wwport_searches[slot].used = false;
	}
	return 0;
}

} /* extern "C" */


/*
**	Win32 file API on POSIX -- see the declarations in windows.h.
*/
#include <fcntl.h>

static const intptr_t WWPORT_FILE_TAG  = 0x40000000;
static const intptr_t WWPORT_FILE_MASK = 0x00FFFFFF;

static HANDLE wwport_file_handle(int fd) { return (HANDLE)(WWPORT_FILE_TAG | (intptr_t)fd); }

int wwport_file_fd(HANDLE h)
{
	intptr_t v = (intptr_t)h;
	if (h == INVALID_HANDLE_VALUE || v < 0) return -1;
	if ((v & ~WWPORT_FILE_MASK) != WWPORT_FILE_TAG) return -1;
	return (int)(v & WWPORT_FILE_MASK);
}

static DWORD wwport_errno_to_win32(int e)
{
	switch (e) {
		case ENOENT:  return ERROR_FILE_NOT_FOUND;
		case ENOTDIR: return ERROR_PATH_NOT_FOUND;
		case EACCES:
		case EPERM:
		case EROFS:   return ERROR_ACCESS_DENIED;
		case EBADF:   return ERROR_INVALID_HANDLE;
		case EEXIST:  return ERROR_FILE_EXISTS;
		case ENOSPC:  return ERROR_DISK_FULL;
		default:      return (DWORD)e;
	}
}
static BOOL wwport_fail(void) { SetLastError(wwport_errno_to_win32(errno)); return FALSE; }

/* The engine writes DOS paths; the separators, at least, must become POSIX ones. */
static void wwport_posix_path(const char * in, char * out, size_t outsize)
{
	size_t i = 0;
	for (; in[i] != '\0' && i + 1 < outsize; i++) out[i] = (in[i] == '\\') ? '/' : in[i];
	out[i] = '\0';
}

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, void * security, DWORD disposition, DWORD flags, HANDLE templatefile)
{
	(void)share; (void)security; (void)flags; (void)templatefile;
	if (name == NULL) { SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; }
	char path[1024];
	wwport_posix_path(name, path, sizeof(path));

	int oflags;
	if ((access & GENERIC_READ) && (access & GENERIC_WRITE)) oflags = O_RDWR;
	else if (access & GENERIC_WRITE)                         oflags = O_WRONLY;
	else                                                     oflags = O_RDONLY;
	switch (disposition) {
		case CREATE_NEW:        oflags |= O_CREAT | O_EXCL;  break;
		case CREATE_ALWAYS:     oflags |= O_CREAT | O_TRUNC; break;
		case OPEN_ALWAYS:       oflags |= O_CREAT;           break;
		case TRUNCATE_EXISTING: oflags |= O_TRUNC;           break;
		case OPEN_EXISTING:
		default:                                             break;
	}
	int fd = open(path, oflags | O_CLOEXEC, 0644);
	if (fd < 0) { wwport_fail(); return INVALID_HANDLE_VALUE; }
	if (fd > (int)WWPORT_FILE_MASK) { close(fd); SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
	return wwport_file_handle(fd);
}

BOOL ReadFile(HANDLE file, LPVOID buffer, DWORD toread, LPDWORD read, LPOVERLAPPED overlapped)
{
	(void)overlapped;
	int fd = wwport_file_fd(file);
	if (read) *read = 0;
	if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
	DWORD total = 0;
	while (total < toread) {						/* read() may return short counts */
		ssize_t n = ::read(fd, (char *)buffer + total, toread - total);
		if (n < 0) { if (errno == EINTR) continue; if (read) *read = total; return wwport_fail(); }
		if (n == 0) break;							/* end of file */
		total += (DWORD)n;
	}
	if (read) *read = total;
	return TRUE;
}

BOOL WriteFile(HANDLE file, LPCVOID buffer, DWORD towrite, LPDWORD written, LPOVERLAPPED overlapped)
{
	(void)overlapped;
	int fd = wwport_file_fd(file);
	if (written) *written = 0;
	if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
	DWORD total = 0;
	while (total < towrite) {
		ssize_t n = ::write(fd, (const char *)buffer + total, towrite - total);
		if (n < 0) { if (errno == EINTR) continue; if (written) *written = total; return wwport_fail(); }
		total += (DWORD)n;
	}
	if (written) *written = total;
	return TRUE;
}

DWORD SetFilePointer(HANDLE file, LONG distance, LONG * distancehigh, DWORD method)
{
	int fd = wwport_file_fd(file);
	if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_SET_FILE_POINTER; }
	off_t offset = distancehigh ? (off_t)(((int64_t)*distancehigh << 32) | (uint32_t)distance) : (off_t)distance;
	int whence = (method == FILE_CURRENT) ? SEEK_CUR : (method == FILE_END) ? SEEK_END : SEEK_SET;
	off_t pos = lseek(fd, offset, whence);
	if (pos < 0) { wwport_fail(); return INVALID_SET_FILE_POINTER; }
	if (distancehigh) *distancehigh = (LONG)((int64_t)pos >> 32);
	return (DWORD)pos;
}

DWORD GetFileSize(HANDLE file, LPDWORD sizehigh)
{
	int fd = wwport_file_fd(file);
	struct stat st;
	if (fd < 0 || fstat(fd, &st) != 0) { SetLastError(ERROR_INVALID_HANDLE); return INVALID_FILE_SIZE; }
	if (sizehigh) *sizehigh = (DWORD)((uint64_t)st.st_size >> 32);
	return (DWORD)st.st_size;
}

BOOL DeleteFileA(LPCSTR name)
{
	if (name == NULL) { SetLastError(ERROR_FILE_NOT_FOUND); return FALSE; }
	char path[1024];
	wwport_posix_path(name, path, sizeof(path));
	return unlink(path) == 0 ? TRUE : wwport_fail();
}

/* FILETIME: 100ns ticks since 1601-01-01 UTC. 11644473600 s separate 1601 from 1970. */
static const int64_t WWPORT_EPOCH_DIFF_100NS = 116444736000000000LL;

static FILETIME wwport_to_filetime(struct timespec ts)
{
	int64_t ticks = (int64_t)ts.tv_sec * 10000000LL + ts.tv_nsec / 100 + WWPORT_EPOCH_DIFF_100NS;
	FILETIME ft;
	ft.dwLowDateTime  = (DWORD)(ticks & 0xFFFFFFFF);
	ft.dwHighDateTime = (DWORD)((uint64_t)ticks >> 32);
	return ft;
}
static struct timespec wwport_from_filetime(const FILETIME * ft)
{
	int64_t ticks = ((int64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	ticks -= WWPORT_EPOCH_DIFF_100NS;
	struct timespec ts;
	ts.tv_sec  = (time_t)(ticks / 10000000LL);
	ts.tv_nsec = (long)((ticks % 10000000LL) * 100);
	return ts;
}

BOOL GetFileInformationByHandle(HANDLE file, LPBY_HANDLE_FILE_INFORMATION info)
{
	int fd = wwport_file_fd(file);
	struct stat st;
	if (fd < 0 || info == NULL || fstat(fd, &st) != 0) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
	memset(info, 0, sizeof(*info));
	info->dwFileAttributes = S_ISDIR(st.st_mode) ? 0x10 : 0x80;		/* DIRECTORY : NORMAL */
	info->ftCreationTime   = wwport_to_filetime(st.st_birthtimespec);
	info->ftLastAccessTime = wwport_to_filetime(st.st_atimespec);
	info->ftLastWriteTime  = wwport_to_filetime(st.st_mtimespec);
	info->nFileSizeHigh    = (DWORD)((uint64_t)st.st_size >> 32);
	info->nFileSizeLow     = (DWORD)st.st_size;
	info->nNumberOfLinks   = (DWORD)st.st_nlink;
	info->nFileIndexHigh   = (DWORD)((uint64_t)st.st_ino >> 32);
	info->nFileIndexLow    = (DWORD)st.st_ino;
	return TRUE;
}

BOOL SetFileTime(HANDLE file, const FILETIME * creation, const FILETIME * access, const FILETIME * write)
{
	(void)creation;									/* POSIX cannot set birth time */
	int fd = wwport_file_fd(file);
	if (fd < 0) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
	struct timespec times[2];
	times[0].tv_nsec = UTIME_OMIT;
	times[1].tv_nsec = UTIME_OMIT;
	if (access) times[0] = wwport_from_filetime(access);
	if (write)  times[1] = wwport_from_filetime(write);
	return futimens(fd, times) == 0 ? TRUE : wwport_fail();
}

/* DOS date/time is LOCAL time, packed as in struct find_t (see wwcompat.h). */
BOOL FileTimeToDosDateTime(const FILETIME * filetime, WORD * dosdate, WORD * dostime)
{
	if (filetime == NULL || dosdate == NULL || dostime == NULL) return FALSE;
	struct timespec ts = wwport_from_filetime(filetime);
	struct tm parts;
	localtime_r(&ts.tv_sec, &parts);
	int year = parts.tm_year + 1900 - 1980;
	if (year < 0 || year > 127) return FALSE;		/* DOS dates span 1980-2107 */
	*dosdate = (WORD)((year << 9) | ((parts.tm_mon + 1) << 5) | parts.tm_mday);
	*dostime = (WORD)((parts.tm_hour << 11) | (parts.tm_min << 5) | (parts.tm_sec / 2));
	return TRUE;
}

BOOL DosDateTimeToFileTime(WORD dosdate, WORD dostime, LPFILETIME filetime)
{
	if (filetime == NULL) return FALSE;
	struct tm parts;
	memset(&parts, 0, sizeof(parts));
	parts.tm_year  = (dosdate >> 9) + 1980 - 1900;
	parts.tm_mon   = ((dosdate >> 5) & 0x0F) - 1;
	parts.tm_mday  = dosdate & 0x1F;
	parts.tm_hour  = dostime >> 11;
	parts.tm_min   = (dostime >> 5) & 0x3F;
	parts.tm_sec   = (dostime & 0x1F) * 2;
	parts.tm_isdst = -1;
	struct timespec ts;
	ts.tv_sec  = mktime(&parts);
	ts.tv_nsec = 0;
	if (ts.tv_sec == (time_t)-1) return FALSE;
	*filetime = wwport_to_filetime(ts);
	return TRUE;
}

/*
**	SetErrorMode: on Windows this suppressed the system's critical-error dialog
**	("insert disk in drive D:") around file reads. macOS raises no such dialog,
**	so there is nothing to suppress; the mode is kept only so the previous value
**	can be returned, which is all callers rely on.
*/
static UINT wwport_error_mode = 0;
UINT SetErrorMode(UINT mode)
{
	UINT previous = wwport_error_mode;
	wwport_error_mode = mode;
	return previous;
}


/*
**	Multimedia timers on GCD -- see port/compat/mmsystem.h.
*/
#include "mmsystem.h"
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>

struct wwport_mmtimer {
	UINT              id;
	dispatch_source_t source;
};
static wwport_mmtimer   wwport_mmtimers[32];
static UINT             wwport_mmtimer_next_id = 1;
static pthread_mutex_t  wwport_mmtimer_lock = PTHREAD_MUTEX_INITIALIZER;
static dispatch_queue_t wwport_mmtimer_queue;
static int              wwport_mmtimer_queue_key;

/* One serial, high-priority queue for every timer: Win32's single timer thread. */
static dispatch_queue_t wwport_timer_queue(void)
{
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		dispatch_queue_attr_t attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0);
		wwport_mmtimer_queue = dispatch_queue_create("com.westwood.redalert.mmtimer", attr);
		dispatch_queue_set_specific(wwport_mmtimer_queue, &wwport_mmtimer_queue_key, &wwport_mmtimer_queue_key, NULL);
	});
	return wwport_mmtimer_queue;
}

MMRESULT timeBeginPeriod(UINT period) { (void)period; return TIMERR_NOERROR; }	/* GCD timers are already ms-accurate */
MMRESULT timeEndPeriod(UINT period)   { (void)period; return TIMERR_NOERROR; }

DWORD timeGetTime(void)
{
	static mach_timebase_info_data_t tb;
	if (tb.denom == 0) mach_timebase_info(&tb);
	return (DWORD)((mach_absolute_time() * tb.numer / tb.denom) / 1000000ULL);
}

MMRESULT timeSetEvent(UINT delay_ms, UINT resolution_ms, LPTIMECALLBACK callback, DWORD user, UINT flags)
{
	if (callback == NULL || delay_ms == 0) return 0;
	pthread_mutex_lock(&wwport_mmtimer_lock);
	int slot = -1;
	for (int i = 0; i < (int)(sizeof(wwport_mmtimers) / sizeof(wwport_mmtimers[0])); i++) {
		if (wwport_mmtimers[i].id == 0) { slot = i; break; }
	}
	if (slot < 0) { pthread_mutex_unlock(&wwport_mmtimer_lock); return 0; }
	UINT id = wwport_mmtimer_next_id++;
	if (wwport_mmtimer_next_id == 0) wwport_mmtimer_next_id = 1;

	dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, wwport_timer_queue());
	uint64_t interval = (uint64_t)delay_ms * NSEC_PER_MSEC;
	uint64_t leeway   = (uint64_t)(resolution_ms ? resolution_ms : 1) * NSEC_PER_MSEC / 2;
	bool periodic = (flags & TIME_PERIODIC) != 0;
	dispatch_source_set_timer(src, dispatch_time(DISPATCH_TIME_NOW, (int64_t)interval),
	                          periodic ? interval : DISPATCH_TIME_FOREVER, leeway);
	dispatch_source_set_event_handler(src, ^{
		callback(id, 0, user, 0, 0);
		if (!periodic) dispatch_source_cancel(src);
	});
	wwport_mmtimers[slot].id = id;
	wwport_mmtimers[slot].source = src;
	pthread_mutex_unlock(&wwport_mmtimer_lock);
	dispatch_resume(src);
	return id;
}

MMRESULT timeKillEvent(UINT id)
{
	dispatch_source_t src = NULL;
	pthread_mutex_lock(&wwport_mmtimer_lock);
	for (int i = 0; i < (int)(sizeof(wwport_mmtimers) / sizeof(wwport_mmtimers[0])); i++) {
		if (id != 0 && wwport_mmtimers[i].id == id) {
			src = wwport_mmtimers[i].source;
			wwport_mmtimers[i].id = 0;
			wwport_mmtimers[i].source = NULL;
			break;
		}
	}
	pthread_mutex_unlock(&wwport_mmtimer_lock);
	if (src == NULL) return TIMERR_NOCANDO;
	dispatch_source_cancel(src);
	/*
	**	Wait for any callback already running to finish, so the caller can free
	**	what the callback uses -- unless we ARE the timer thread, where waiting on
	**	our own queue would deadlock.
	*/
	if (dispatch_get_specific(&wwport_mmtimer_queue_key) == NULL) {
		dispatch_sync(wwport_timer_queue(), ^{});
	}
	dispatch_release(src);
	return TIMERR_NOERROR;
}

HANDLE GetCurrentProcess(void) { return (HANDLE)(intptr_t)-1; }	/* Win32's pseudo-handle values */
HANDLE GetCurrentThread(void)  { return (HANDLE)(intptr_t)-2; }
BOOL DuplicateHandle(HANDLE srcprocess, HANDLE src, HANDLE dstprocess, LPHANDLE dst, DWORD access, BOOL inherit, DWORD options)
{
	(void)srcprocess; (void)dstprocess; (void)access; (void)inherit; (void)options;
	if (dst) *dst = src;
	return TRUE;
}
BOOL  SetPriorityClass(HANDLE process, DWORD priorityclass) { (void)process; (void)priorityclass; return TRUE; }
DWORD GetPriorityClass(HANDLE process) { (void)process; return NORMAL_PRIORITY_CLASS; }

/* See windows.h: the timer queue already runs at the highest QoS class. */
BOOL SetThreadPriority(HANDLE thread, int priority) { (void)thread; (void)priority; return TRUE; }
int  GetThreadPriority(HANDLE thread) { (void)thread; return THREAD_PRIORITY_NORMAL; }

/* CD volume label: see windows.h -- a Mac has no drive letters. */
BOOL GetVolumeInformationA(LPCSTR root, char * volname, DWORD volnamesize, LPDWORD serial, LPDWORD maxcomponent, LPDWORD flags, char * fsname, DWORD fsnamesize)
{
	(void)root; (void)volname; (void)volnamesize; (void)serial; (void)maxcomponent; (void)flags; (void)fsname; (void)fsnamesize;
	SetLastError(ERROR_PATH_NOT_FOUND);
	return FALSE;
}

/* Disk free space: see wwcompat.h for why it is capped. */
#include <sys/statvfs.h>
extern "C" {
unsigned _dos_getdiskfree(unsigned drive, struct diskfree_t * result)
{
	(void)drive;
	if (result == NULL) return EINVAL;
	struct statvfs fs;
	uint64_t avail = (statvfs(".", &fs) == 0) ? (uint64_t)fs.f_bavail * fs.f_frsize : 0;
	const uint64_t cap = 0x7FFFF000ull;				/* < 2GB, a whole number of 4K clusters */
	if (avail > cap) avail = cap;
	result->bytes_per_sector    = 512;
	result->sectors_per_cluster = 8;				/* 4K clusters */
	result->avail_clusters      = (unsigned)(avail / 4096);
	result->total_clusters      = result->avail_clusters;
	return 0;
}
void _dos_getdrive(unsigned * drive) { if (drive) *drive = 3; }
void _dos_setdrive(unsigned drive, unsigned * total) { (void)drive; if (total) *total = 26; }
} /* extern "C" */

/*
**	FindFirstFile / FindNextFile / FindClose -- see windows.h. A search handle is
**	a heap object (never a tagged file descriptor, so CloseHandle ignores it);
**	FindClose frees it.
*/
struct wwport_findfile {
	DIR * dir;
	char  dirpath[1024];
	char  pattern[256];
};

static BOOL wwport_findfile_next(wwport_findfile * f, LPWIN32_FIND_DATAA data)
{
	struct dirent * entry;
	while ((entry = readdir(f->dir)) != NULL) {
		const char * name = entry->d_name;
		if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
		if (strlen(name) >= sizeof(data->cFileName)) continue;
		if (!wwport_dos_match(f->pattern, name)) continue;
		char full[1400];
		snprintf(full, sizeof(full), "%s/%s", f->dirpath, name);
		struct stat st;
		if (stat(full, &st) != 0) continue;
		memset(data, 0, sizeof(*data));
		DWORD attr = 0;
		if (S_ISDIR(st.st_mode))      attr |= FILE_ATTRIBUTE_DIRECTORY;
		if (name[0] == '.')           attr |= FILE_ATTRIBUTE_HIDDEN;
		if (access(full, W_OK) != 0)  attr |= FILE_ATTRIBUTE_READONLY;
		data->dwFileAttributes = attr ? attr : FILE_ATTRIBUTE_NORMAL;
		data->ftCreationTime   = wwport_to_filetime(st.st_birthtimespec);
		data->ftLastAccessTime = wwport_to_filetime(st.st_atimespec);
		data->ftLastWriteTime  = wwport_to_filetime(st.st_mtimespec);
		data->nFileSizeHigh    = (DWORD)((uint64_t)st.st_size >> 32);
		data->nFileSizeLow     = (DWORD)st.st_size;
		strcpy(data->cFileName, name);
		return TRUE;
	}
	SetLastError(ERROR_FILE_NOT_FOUND);			/* ERROR_NO_MORE_FILES on Win32; the engine only tests the BOOL */
	return FALSE;
}

HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
	if (pattern == NULL || data == NULL) { SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; }
	wwport_findfile * f = (wwport_findfile *)calloc(1, sizeof(wwport_findfile));
	if (f == NULL) return INVALID_HANDLE_VALUE;
	char path[1024];
	wwport_posix_path(pattern, path, sizeof(path));
	const char * slash = strrchr(path, '/');
	if (slash) {
		size_t n = (size_t)(slash - path);
		memcpy(f->dirpath, path, n); f->dirpath[n] = '\0';
		if (n == 0) strcpy(f->dirpath, "/");
		snprintf(f->pattern, sizeof(f->pattern), "%s", slash + 1);
	} else {
		strcpy(f->dirpath, ".");
		snprintf(f->pattern, sizeof(f->pattern), "%s", path);
	}
	f->dir = opendir(f->dirpath);
	if (f->dir == NULL || !wwport_findfile_next(f, data)) {
		if (f->dir) closedir(f->dir);
		free(f);
		SetLastError(ERROR_FILE_NOT_FOUND);
		return INVALID_HANDLE_VALUE;
	}
	return (HANDLE)f;
}

BOOL FindNextFileA(HANDLE search, LPWIN32_FIND_DATAA data)
{
	if (search == INVALID_HANDLE_VALUE || search == NULL || data == NULL) return FALSE;
	return wwport_findfile_next((wwport_findfile *)search, data);
}

BOOL FindClose(HANDLE search)
{
	if (search == INVALID_HANDLE_VALUE || search == NULL) return FALSE;
	wwport_findfile * f = (wwport_findfile *)search;
	if (f->dir) closedir(f->dir);
	free(f);
	return TRUE;
}
