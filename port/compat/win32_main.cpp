/*
**	win32_main.cpp -- the process entry point: a Mac main() that starts the
**	game's own WinMain (CODE/STARTUP.CPP) as Windows would have.
**
**	Engine side: built with the engine's flags.
**
**	Where the game's data lives: WinMain changes to the directory its .EXE is
**	in and expects the data there. Here that directory is $RA_DATA_DIR if set,
**	otherwise the folder holding this executable. main() changes to it first,
**	and GetModuleFileName (win32_system.cpp) then reports "./RA95.EXE", so the
**	game's own chdir is a no-op -- which also keeps a long Mac path out of the
**	132-byte buffer WinMain keeps it in.
*/

#include "windows.h"
#include "ra_platform.h"

#include <mach-o/dyld.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <strings.h>
#include <unistd.h>

int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, char * command_line, int command_show);

static std::string data_directory(void)
{
	char const * env = getenv("RA_DATA_DIR");
	if (env != NULL && env[0] != 0) return env;
	char path[4096];
	uint32_t len = sizeof(path);
	if (_NSGetExecutablePath(path, &len) != 0) return ".";
	char resolved[PATH_MAX];
	if (realpath(path, resolved) == NULL) return ".";
	return dirname(resolved);
}

int main(int argc, char * argv[])
{
	std::string dir = data_directory();
	if (chdir(dir.c_str()) != 0) {
		fprintf(stderr, "Red Alert: cannot open the data directory %s\n", dir.c_str());
		return EXIT_FAILURE;
	}

	/*
	**	The Windows command line: the arguments after the program name,
	**	space-separated. Unless the player gave their own, "-CD." is added:
	**	Westwood's option for playing from a hard disk (INIT.CPP), which makes
	**	the data folder the search path and every file local -- so the game
	**	never asks for a CD.
	*/
	std::string line;
	bool search_path = false;
	for (int i = 1; i < argc; i++) {
		if (!line.empty()) line += ' ';
		line += argv[i];
		if (strncasecmp(argv[i], "-CD", 3) == 0) search_path = true;
	}
	if (!search_path) line = line.empty() ? std::string("-CD.") : line + " -CD.";
	static char command_line[1024];
	snprintf(command_line, sizeof(command_line), "%s", line.c_str());

	RA_Platform_Init();
	return WinMain((HINSTANCE)1, NULL, command_line, SW_SHOWNORMAL);
}
