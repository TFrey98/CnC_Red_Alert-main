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
**	STMPCACH.CPP -- PORT-CREATED. C translation of STMPCACH.ASM:
**	Cache_Copy_Icon and Is_Icon_Cached, the DirectDraw video-memory icon
**	cache's lookups. Verified against the original assembly:
**	port/tests/asm_drawbuff.cpp.
**
**	The assembly used Watcom's register calling convention (the trailing-
**	underscore names) both ways; these are ordinary C functions now.
*/

#include "gbuffer.h"
#include "tile.h"
#include "iconcach.h"
#include "stampvar.h"
#include <string.h>

/* ICON_WIDTH and ICON_HEIGHT (24) come from iconcach.h, as in STMPCACH.ASM. */

/* Defined in ICONCACH.CPP; the assembly declared them itself, as here. */
extern "C" IconSetType	IconSetList[MAX_ICON_SETS];
extern "C" short			IconCacheLookup[MAX_LOOKUP_ENTRIES];

/*
**	Copy one 24 x 24 icon into a surface `pitch` bytes wide.
*/
extern "C" void Cache_Copy_Icon(void const * icon_ptr, void * dest, int pitch)
{
	unsigned char const * s = (unsigned char const *)icon_ptr;
	unsigned char * d = (unsigned char *)dest;
	for (int line = 0; line < ICON_HEIGHT; line++) {
		memcpy(d, s, ICON_WIDTH);
		s += ICON_WIDTH;
		d += pitch;
	}
}

/*
**	The registered icon set holding this icon data (Is_Stamp_Registered_).
*/
static IconSetType * Stamp_Registered(void const * icon_data)
{
	for (int i = 0; i < MAX_ICON_SETS; i++) {
		if (IconSetList[i].IconSetPtr == icon_data) return &IconSetList[i];
	}
	return NULL;
}

/*
**	The cache slot holding this icon, caching it on first use; -1 if it is not
**	(or cannot be) cached. Faithful to the assembly:
**	  * The icon number goes through the set's map, low byte only (as when
**	    drawing), then must be below IconCount -- a SIGNED compare here.
**	  * A negative Get_Free_Cache_Slot result is returned as is.
**	  * The lookup table is indexed by the set's IconListOffset in BYTES.
*/
extern "C" int Is_Icon_Cached(void const * icon_data, int icon)
{
	if (icon_data == NULL) return -1;
	if (LastIconset != icon_data) Init_Stamps(icon_data);
	uint32_t i = (uint32_t)icon;
	if (MapPtr) i = (i & ~0xFFu) | MapPtr[(int32_t)i];
	if (!((int32_t)i < IconCount)) return -1;

	IconSetType * set = Stamp_Registered(icon_data);
	if (set == NULL) return -1;
	short * entry = (short *)((char *)IconCacheLookup + set->IconListOffset) + (int32_t)i;
	if (*entry != -1) return (unsigned short)*entry;

	int slot = Get_Free_Cache_Slot();
	if (slot < 0) return slot;
	if (!Cache_New_Icon(slot, (void *)(StampPtr + (int32_t)(i * (uint32_t)IconSize)))) return -1;
	*entry = (short)slot;
	return slot & 0xFFFF;
}
