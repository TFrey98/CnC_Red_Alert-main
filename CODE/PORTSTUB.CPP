/*
**	Command & Conquer Red Alert(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
**	PORTSTUB.CPP -- PORT-CREATED. Stand-ins for Westwood code the Mac build
**	does not have, each of which only ever served DOS hardware or debugging.
**	Every one is reached by linked code; none has anything to do on macOS.
*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" {

/*
**	WIN32LIB/PROFILE (dropped): the in-game profiler, stopped at shutdown.
*/
void __cdecl Stop_Profiler(void) {}

/*
**	The movie player's monochrome-monitor debug display (MONODISP.CPP calls
**	these; MONO.ASM is archived): a second, text-only screen on the
**	developers' machines.
*/
void __cdecl Mono_Enable(void) {}
void __cdecl Mono_Clear_Screen(void) {}
void __cdecl Mono_Set_Cursor(long x, long y) {(void)x; (void)y;}
void __cdecl Mono_Print(void const * text) {(void)text;}
void __cdecl Mono_Draw_Rect(long x, long y, long w, long h, long attrib, long thick)
{
	(void)x; (void)y; (void)w; (void)h; (void)attrib; (void)thick;
}

/*
**	Touched every page of a buffer so DOS virtual memory would page it in before
**	time-critical drawing (EGOS.CPP, the movie loader). Nothing pages here.
*/
void __cdecl Force_VM_Page_In(void * buffer, int length) {(void)buffer; (void)length;}

/*
**	The movie player's MCGA blitters (MCGABUF.ASM, archived), used only by its
**	direct-to-VGA mode; the game asks for buffer mode instead (INIT.CPP:
**	VQACFGF_BUFFER), so the direct path is linked but not taken.
**
**	MCGA_Blit is a plain rectangle copy into a 320-wide screen, translated as
**	such. MCGA_BufferCopy's only destination was the VGA framebuffer at
**	A0000h, which a Mac does not have: it does nothing.
*/
void __cdecl MCGA_Blit(unsigned char * buffer, unsigned char * screen, long imgwidth, long imgheight)
{
	for (long row = 0; row < imgheight; row++) {
		memcpy(screen, buffer, (size_t)imgwidth);
		buffer += imgwidth;
		screen += 320;
	}
}

void __cdecl MCGA_BufferCopy(unsigned char * buffer, unsigned char * dummy) {(void)buffer; (void)dummy;}

}

/*
**	MPGDLL (a binary DLL, mpgdll.lib in CODE/MAKEFILE): MPEG movies for the DVD
**	edition. CONQUER.CPP takes that path only when Using_DVD() -- the registry's
**	"DVD" value, which is not set on the Mac (port/compat/win32_system.cpp) --
**	so the VQA movies are always used and these are never reached. If MPEG
**	movies are ever wanted, AVFoundation is the native route.
*/
struct IDirectDraw;
struct IDirectDrawSurface;
extern "C" {
void __stdcall MpgPlay(const char * name, IDirectDraw * dd, IDirectDrawSurface * surface, void * dstRect)
{
	(void)name; (void)dd; (void)surface; (void)dstRect;
}
void __stdcall MpgPause(void) {}
void __stdcall MpgResume(void) {}
void __stdcall MpgSetCallback(void * callback, void * user) {(void)callback; (void)user;}
}

/*
**	INIT.CPP stores its random seed here for the assembly random-number
**	generator (RANDOM.ASM), which nothing in the linked game calls.
*/
long RandNumb = 0;
