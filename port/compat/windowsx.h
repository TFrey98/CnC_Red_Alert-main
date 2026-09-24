/* windowsx.h -- Win32 convenience macros. */
#ifndef WWPORT_COMPAT_WINDOWSX_H
#define WWPORT_COMPAT_WINDOWSX_H
#include "windows.h"
#define GET_X_LPARAM(lp) ((int)(short)((lp) & 0xffff))
#define GET_Y_LPARAM(lp) ((int)(short)(((lp) >> 16) & 0xffff))
#endif
