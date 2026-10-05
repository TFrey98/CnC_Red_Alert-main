/* dos.h -- residual DOS-era declarations. Segmented/interrupt services are gone. */
#ifndef WWPORT_COMPAT_DOS_H
#define WWPORT_COMPAT_DOS_H
#include <stdlib.h>
#include <string.h>
/*
**	Watcom's DOS critical-error ("Abort, Retry, Fail?") handler results.
**	CDFILE.CPP's harderr_handler returns one; on macOS nothing installs it.
*/
#define _HARDERR_IGNORE 0
#define _HARDERR_RETRY  1
#define _HARDERR_ABORT  2
#define _HARDERR_FAIL   3

#endif
