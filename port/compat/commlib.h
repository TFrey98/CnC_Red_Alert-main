/*
**	commlib.h -- stub for the Greenleaf Communications Library serial API.
**
**	See modem.h. Proprietary, absent from EA's release, serial-multiplayer only.
**
**	PORT is declared as an OPAQUE type rather than left out entirely. The engine
**	names it in four places that are not behind `#ifdef WIN32` -- nullmgr.h:164
**	`Abort_Modem(PORT *)`, nullmgr.h:193 and nullconn.h:132 `PORT *Port;`, and
**	NULLMGR.CPP:2338 -- but every one of them uses it only as a pointer, which
**	an incomplete type satisfies.
**
**	Deliberately left undefined: any code that tries to dereference a PORT or
**	take its size fails at that line with a clear "incomplete type" error,
**	rather than silently compiling against a fabricated layout.
*/
#ifndef WWPORT_COMPAT_COMMLIB_H
#define WWPORT_COMPAT_COMMLIB_H

typedef struct wwport_gcl_port_opaque PORT;

#endif
