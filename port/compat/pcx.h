/*
**	pcx.h -- the PCX reader/writer under its original name.
**
**	Not a stub. CODE/function.h includes "pcx.h", but the release ships this
**	header as FILEPCX.H; note that FILEPCX.H's own include guard is still
**	`PCX_H`, which is the leftover evidence of the rename.
**
**	CODE/FILEPCX.H and WIN32LIB/INCLUDE/FILEPCX.H are byte-for-byte identical,
**	so the usual -iquote/-I split does not matter here. The angle-bracket form
**	below resolves to the WIN32LIB copy, the library that actually implements
**	these functions.
*/
#ifndef WWPORT_COMPAT_PCX_H
#define WWPORT_COMPAT_PCX_H

#include <filepcx.h>

#endif
