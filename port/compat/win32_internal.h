/*
**	win32_internal.h -- shared by the compat layer's own .cpp files: what one
**	part of the Win32 emulation needs from another. Not for engine code.
*/
#ifndef WIN32_INTERNAL_H
#define WIN32_INTERNAL_H

#include "ra_platform.h"

/* The Mac window behind the game's main window (win32_window.cpp), for DirectDraw. */
RA_Display * WWPort_Main_Display(void);

/* Present the DirectDraw primary if it changed (win32_ddraw.cpp); called by the message pump. */
void WWPort_Display_Pump(void);

#endif
