/*
** fake_platform.cpp -- scripted stand-in for the native backend in tests.
**
** RA_Platform_Disk_Error answers from $RA_TEST_ANSWERS ("R" = Try Again,
** "C" = Cancel), one character per call, and counts calls in $RA_TEST_COUNT_FILE
** so a parent process can check how many times the player was asked even if the
** child exits on Cancel. The real implementation (port/backend/ra_dialog.mm) shows
** an NSAlert, which an automated test must never do.
*/
#include "ra_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int RA_Platform_Disk_Error(const char * filename, int error_code)
{
	static int call = 0;
	const char * answers = getenv("RA_TEST_ANSWERS");
	const char * countfile = getenv("RA_TEST_COUNT_FILE");
	int index = call++;
	if (countfile) { FILE * f = fopen(countfile, "w"); if (f) { fprintf(f, "%d\n", call); fclose(f); } }
	(void)filename; (void)error_code;
	if (answers == NULL || index >= (int)strlen(answers)) return RA_DISK_ERROR_CANCEL;
	return answers[index] == 'R' ? RA_DISK_ERROR_RETRY : RA_DISK_ERROR_CANCEL;
}

/*
** The audio device: "starts" at $RA_TEST_AUDIO_RATE (default 22050) and never
** calls back -- tests drive the mixer themselves (WWPort_DSound_Mix).
*/
int RA_Audio_Start(RA_Audio_Render render, void * user, int * rate)
{
	(void)render; (void)user;
	const char * r = getenv("RA_TEST_AUDIO_RATE");
	if (rate) *rate = r ? atoi(r) : 22050;
	return 1;
}
void RA_Audio_Stop(void) {}

/* MessageBox: never shown in a test; answers the first button. */
int RA_Platform_Message_Box(const char * text, const char * caption, int buttons, int warning)
{
	(void)text; (void)caption; (void)warning;
	return buttons == RA_MB_YESNO ? RA_ID_YES : RA_ID_OK;
}
