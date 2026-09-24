/*
**	modem.h -- stub for the Greenleaf Communications Library (GCL).
**
**	GCL is proprietary, was never shipped with EA's source release, and drove
**	the serial/modem multiplayer path only. CODE/function.h includes it
**	unconditionally, so every translation unit in the game needs the header to
**	exist even though single-player touches none of its entry points.
**
**	This stub is deliberately EMPTY rather than a set of fake prototypes. A
**	missing declaration fails loudly at the point of use, which is what we want:
**	it marks exactly which code is still on the modem path. Fake prototypes
**	would compile and then fail at link time with no indication of why.
**
**	Restoring serial multiplayer means replacing this with a real serial
**	implementation (termios on macOS), not filling in Greenleaf's API.
*/
#ifndef WWPORT_COMPAT_MODEM_H
#define WWPORT_COMPAT_MODEM_H
#endif
