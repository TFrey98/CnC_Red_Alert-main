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
**	STAMPVAR.H -- PORT-CREATED. The icon-set state STAMP.ASM kept in its data
**	segment and exported to STMPCACH.ASM; now defined in STAMP.CPP and shared
**	by the two translations.
*/
#ifndef STAMPVAR_H
#define STAMPVAR_H

#include <stdint.h>

extern "C" {
extern void const *				LastIconset;
extern unsigned char const *	StampPtr;
extern unsigned char const *	IsTrans;
extern unsigned char const *	MapPtr;
extern int32_t						IconWidth;
extern int32_t						IconHeight;
extern int32_t						IconSize;
extern int32_t						IconCount;
void Init_Stamps(void const * icondata);
}

bool WWPort_Stamp_Index(int & icon);

#endif
