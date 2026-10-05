/*
**	win32_system.cpp -- Win32 process, system-information and registry calls
**	the engine makes, over POSIX/Darwin.
**
**	Engine side: built with the engine's flags. No Cocoa.
*/

#include "windows.h"

#include <mach-o/dyld.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

/* ---------------------------------------------------------------- process */

/*
**	"The .EXE": STARTUP.CPP makes this argv[0] and changes to its directory,
**	expecting the game's data there. main() (win32_main.cpp) has already
**	changed to the data directory, so the .EXE is reported as being in it.
*/
DWORD GetModuleFileNameA(HMODULE module, LPSTR buffer, DWORD size)
{
	(void)module;
	if (buffer == NULL || size == 0) return 0;
	strncpy(buffer, "./RA95.EXE", size - 1);
	buffer[size - 1] = 0;
	return (DWORD)strlen(buffer);
}

/*
**	Windows 95, version 4.0. The engine's only question (Get_OS_Version) is
**	"NT or not" -- high bit set means not -- and it took the 95 paths, which
**	are the ones the port follows.
*/
DWORD GetVersion(void)
{
	return 0x80000004u;
}

void ExitProcess(UINT code)
{
	exit((int)code);
}

/*
**	Real memory figures, capped at 2GB: the engine compares these with 32-bit
**	arithmetic in places (2KEYFRAM.CPP decides whether to cache shapes).
*/
void GlobalMemoryStatus(LPMEMORYSTATUS status)
{
	if (status == NULL) return;
	uint64_t total = 0;
	size_t len = sizeof(total);
	sysctlbyname("hw.memsize", &total, &len, NULL, 0);
	SIZE_T const cap = 0x7FFFFFFF;
	SIZE_T t = total > cap ? cap : (SIZE_T)total;
	memset(status, 0, sizeof(*status));
	status->dwLength = sizeof(*status);
	status->dwMemoryLoad = 50;
	status->dwTotalPhys = t;
	status->dwAvailPhys = t / 2;
	status->dwTotalPageFile = t;
	status->dwAvailPageFile = t / 2;
	status->dwTotalVirtual = cap;
	status->dwAvailVirtual = cap;
}

UINT GetDriveTypeA(LPCSTR root)
{
	(void)root;
	return DRIVE_NO_ROOT_DIR;
}

/* --------------------------------------------------------------- registry */

/*
**	The Windows registry, as far as the engine reads it. On Windows the
**	installer wrote these; the Mac has no installer, so they are answered the
**	way the installer would have set them:
**
**	  CStrikeInstalled   1 if EXPAND.MIX is present  (the Counterstrike data)
**	  AftermathInstalled 1 if EXPAND2.MIX is present (the Aftermath data)
**	  DVD                not present (the DVD edition's layout is not this one)
**
**	-- the same files Westwood's own commented-out checks looked for
**	(CONQUER.CPP, Is_Counterstrike_Installed / Is_Aftermath_Installed).
**	Anything else (Westwood Chat install paths, the WOLAPI setup flag) is not
**	found, as on a machine without those programs.
*/
namespace {
struct OpenKey {int unused;};
OpenKey GameKey;

bool data_file_present(char const * name)
{
	HANDLE h = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	CloseHandle(h);
	return true;
}
}

LONG RegOpenKeyExA(HKEY key, LPCSTR subkey, DWORD options, DWORD desired, PHKEY result)
{
	(void)key; (void)subkey; (void)options; (void)desired;
	if (result == NULL) return ERROR_INVALID_PARAMETER;
	*result = (HKEY)&GameKey;
	return ERROR_SUCCESS;
}

LONG RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD cbdata)
{
	(void)reserved;
	if (key != (HKEY)&GameKey || name == NULL) return ERROR_FILE_NOT_FOUND;
	DWORD value;
	if (strcasecmp(name, "CStrikeInstalled") == 0) {
		value = data_file_present("EXPAND.MIX") ? 1 : 0;
	} else if (strcasecmp(name, "AftermathInstalled") == 0) {
		value = data_file_present("EXPAND2.MIX") ? 1 : 0;
	} else {
		return ERROR_FILE_NOT_FOUND;
	}
	if (type) *type = REG_DWORD;
	if (data == NULL || cbdata == NULL || *cbdata < sizeof(DWORD)) {
		if (cbdata) *cbdata = sizeof(DWORD);
		return data ? ERROR_MORE_DATA : ERROR_SUCCESS;
	}
	memcpy(data, &value, sizeof(DWORD));
	*cbdata = sizeof(DWORD);
	return ERROR_SUCCESS;
}

LONG RegDeleteValueA(HKEY key, LPCSTR name) {(void)key; (void)name; return ERROR_FILE_NOT_FOUND;}
LONG RegCloseKey(HKEY key) {(void)key; return ERROR_SUCCESS;}
