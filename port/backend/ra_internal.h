/*
**	ra_internal.h -- shared between the backend's own .mm files only. Never
**	included by engine code (that is what ra_platform.h is for).
*/
#ifndef RA_INTERNAL_H
#define RA_INTERNAL_H

#include "ra_platform.h"

void RA_Input_Push(RA_Event const & e);			/* ra_input.mm */
int  RA_Input_VK_From_Mac(unsigned short code);	/* ra_input.mm; 0 = no Windows key */

#endif
