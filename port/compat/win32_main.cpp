/*
**	win32_main.cpp -- the process entry point: a Mac main() that starts the
**	game's own WinMain (CODE/STARTUP.CPP) as Windows would have.
**
**	Engine side: built with the engine's flags.
**
**	Where the game's data lives: WinMain changes to the directory its .EXE is
**	in and expects the data there. Here main() finds the data folder first and
**	changes to it, and GetModuleFileName (win32_system.cpp) then reports
**	"./RA95.EXE", so the game's own chdir is a no-op -- which also keeps a long
**	Mac path out of the 132-byte buffer WinMain keeps it in.
**
**	The data folder is the first of:
**	  1. $RA_DATA_DIR, used as given;
**	  2. the folder holding this executable, or the folder holding the .app it
**	     is inside, if REDALERT.MIX is there (the latter is then saved);
**	  3. the folder chosen at an earlier launch;
**	  4. an installed copy found in the usual places (find_installed_data),
**	     which is then saved for next time;
**	  5. a folder the player picks now, which is then saved for next time.
**	Holding Option at launch skips 3 and 4, to choose a different folder.
*/

#include "windows.h"
#include "ra_platform.h"

#include <mach-o/dyld.h>
#include <glob.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <vector>

int PASCAL WinMain(HINSTANCE instance, HINSTANCE previous, char * command_line, int command_show);

static bool has_data(std::string const & dir)
{
	return !dir.empty() && access((dir + "/REDALERT.MIX").c_str(), R_OK) == 0;
}

static std::string executable_directory(void)
{
	char path[4096];
	uint32_t len = sizeof(path);
	if (_NSGetExecutablePath(path, &len) != 0) return "";
	char resolved[PATH_MAX];
	if (realpath(path, resolved) == NULL) return "";
	return dirname(resolved);
}

/*
**	Looks for an installed copy of the game: the Steam or EA App release in a
**	CrossOver, Whisky or Wine bottle, or a native Steam download. Returns the
**	first folder found that holds REDALERT.MIX, or "".
*/
static std::string find_installed_data(void)
{
	char const * home = getenv("HOME");
	if (home == NULL || home[0] == 0) return "";

	/*
	**	Windows drives inside the usual bottles, and below each the install
	**	folder at the depths the stores use: "Program Files (x86)/Steam/
	**	steamapps/common/<game>" is four levels, "Program Files/EA Games/<game>"
	**	two.
	*/
	char const * drives[] = {
		"/Library/Application Support/CrossOver/Bottles/*/drive_c",
		"/Library/Containers/com.isaacmarovitz.Whisky/Bottles/*/drive_c",
		"/.wine/drive_c",
	};
	char const * installs[] = {
		"/Program Files*/*/",
		"/Program Files*/*/*/",
		"/Program Files*/*/*/*/",
		"/Program Files*/*/*/*/*/",
	};
	std::vector<std::string> patterns;
	for (char const * drive : drives) {
		for (char const * install : installs) patterns.push_back(std::string(home) + drive + install);
	}
	patterns.push_back(std::string(home) + "/Library/Application Support/Steam/steamapps/common/*/");

	/*
	**	Folders are globbed and REDALERT.MIX tested with access(), not globbed
	**	itself: glob matches names case-sensitively, the volume usually does not.
	*/
	for (std::string const & pattern : patterns) {
		glob_t found;
		if (glob(pattern.c_str(), 0, NULL, &found) == 0) {
			for (size_t i = 0; i < found.gl_pathc; i++) {
				std::string dir = found.gl_pathv[i];
				while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
				if (has_data(dir)) {
					globfree(&found);
					return dir;
				}
			}
		}
		globfree(&found);
	}
	return "";
}

/*
**	Returns the data folder, or "" if the player cancelled the folder picker.
*/
static std::string data_directory(void)
{
	char const * env = getenv("RA_DATA_DIR");
	if (env != NULL && env[0] != 0) return env;

	std::string exe = executable_directory();
	if (has_data(exe)) return exe;

	/*
	**	The folder the player put the .app in (the drop-in layout of the release
	**	zip). Saved, so the game still finds it if the app is moved later.
	*/
	char folder[4096];
	if (RA_Platform_App_Folder(folder, sizeof(folder)) && has_data(folder)) {
		RA_Platform_Save_Data_Folder(folder);
		return folder;
	}

	if (!RA_Platform_Option_Key_Down()) {
		if (RA_Platform_Saved_Data_Folder(folder, sizeof(folder)) && has_data(folder)) return folder;
		std::string installed = find_installed_data();
		if (!installed.empty()) {
			RA_Platform_Save_Data_Folder(installed.c_str());
			return installed;
		}
	}

	char const * message = "Choose the folder that holds the Red Alert game files (REDALERT.MIX, MAIN1.MIX ...). "
	                       "To type a path, press Shift-Command-G.";
	while (RA_Platform_Choose_Data_Folder(folder, sizeof(folder), message)) {
		if (has_data(folder)) {
			RA_Platform_Save_Data_Folder(folder);
			return folder;
		}
		message = "That folder does not contain REDALERT.MIX. Choose the folder that holds the Red Alert game files. "
		          "To type a path, press Shift-Command-G.";
	}
	return "";
}

int main(int argc, char * argv[])
{
	RA_Platform_Init();		// first: the folder picker needs the application

	std::string dir = data_directory();
	if (dir.empty()) return EXIT_SUCCESS;		// the player cancelled the folder picker
	if (chdir(dir.c_str()) != 0) {
		std::string text = "Red Alert cannot open the data folder " + dir + ".";
		fprintf(stderr, "%s\n", text.c_str());
		RA_Platform_Message_Box(text.c_str(), "Red Alert", RA_MB_OK, 1);
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

	return WinMain((HINSTANCE)1, NULL, command_line, SW_SHOWNORMAL);
}
