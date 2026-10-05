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
**	2KEYFBUF.CPP -- PORT-CREATED. C translation of 2KEYFBUF.ASM:
**	Buffer_Frame_To_Page, which draws every unit, building and effect frame,
**	with the transparent / ghost (translucent) / fading / predator (cloak
**	shimmer) effects. Verified against the original assembly under the x86
**	emulator, both drawing paths and every effect combination:
**	port/tests/asm_game.cpp.
**
**	The assembly has two paths and both are kept, because they do not always
**	agree:
**
**	  OLD -- per pixel, one routine per combination of effects (BF_*). Used
**	         when the shape is not in the big shape buffer, when it needs
**	         clipping, or when its cached line classification was made for
**	         different flags.
**	  NEW -- for shapes in the big shape buffer (UseBigShapeBuffer). On first
**	         draw each line is classified (all transparent, has ghost pixels,
**	         ...) and the result cached in the shape's header; later unclipped
**	         draws with the same flags pick a routine per line from a jump
**	         table (Single_Line_*). The classification uses the ghost table of
**	         the FIRST draw and is never recomputed.
**
**	The NEW path's line routines differ from the OLD in four ways, all
**	reproduced (and all confirmed under the emulator):
**	  * Predator+Transparent: a pixel the shimmer skips is left alone; the OLD
**	    path writes the shape's pixel there.
**	  * Predator+Fading: the shimmer offset is updated with `and ...,2` where
**	    `add ...,2` was meant, so the pattern barely moves.
**	  * Predator+Ghost: the translucency lookup is indexed with the shimmer
**	    offset's high byte still in the register.
**	  * Predator+Ghost+Fading: the register holding the pixel is cleared just
**	    before fading, so every drawn pixel becomes fade(0).
**
**	Divergences, where the assembly could not be followed:
**	  * Predator+Ghost (NEW path) with a partial shimmer: a line's first
**	    unshimmered pixels were looked up with the jump table's ADDRESS in the
**	    register's high bits; here those bits are 0.
**	  * A fading count of 0, or a zero-size shape, made the assembly's
**	    counters wrap (2^32 iterations); here 0 fades are applied / nothing is
**	    drawn.
**	  * Predator+Ghost+Transparent (NEW path) popped one stack word too many
**	    per line, scrambling the caller's saved registers; nothing to
**	    reproduce in C.
*/

#include "function.h"
#include "ww_view.h"
#include <stdarg.h>
#include <string.h>

extern "C" {
extern char *	BigShapeBufferStart;			// 2KEYFRAM.CPP
extern char *	TheaterShapeBufferStart;
extern BOOL		UseBigShapeBuffer;
extern bool		UseOldShapeDraw;				// CONQUER.CPP
}

namespace {

/* The assembly's SHAPE_ values; checked against the game's below. */
enum {
	F_CENTER = 0x0020, F_TRANS = 0x0040, F_FADING = 0x0100, F_PREDATOR = 0x0200,
	F_GHOST = 0x1000, F_PARTIAL = 0x4000,
	BLIT_TRANSPARENT = 1, BLIT_GHOST = 2, BLIT_FADING = 4, BLIT_PREDATOR = 8, BLIT_SKIP = 16, BLIT_ALL = 31,
	PRED_MASK = 0x0E
};

static_assert(F_CENTER == SHAPE_CENTER && F_FADING == SHAPE_FADING && F_PREDATOR == SHAPE_PREDATOR
	&& F_GHOST == SHAPE_GHOST && F_PARTIAL == SHAPE_PARTIAL,
	"2KEYFBUF.ASM's SHAPE_ EQUs must equal ShapeFlags_Type (SHAPE.H); SHAPE_TRANS (0x40) is CONQUER.CPP's #define");

struct ShapeHeader {			// 2KEYFRAM.CPP's ShapeHeaderType, as the assembly saw it
	uint32_t	draw_flags;
	int32_t	shape_data;
	int32_t	shape_buffer;
};

/*
**	The predator ("shimmer") state: the assembly's data segment, kept across
**	calls. BFPredOffset's low byte steps 0,2,..,14 through an 8-entry table of
**	offsets to a nearby destination pixel; a negative predator argument sets
**	its upper bits, which makes the table read land (by the data layout) on
**	BFPredNegTable[8..15]: offsets one line down and to the left.
*/
uint32_t BFPredOffset;
uint32_t BFPartialPred;
uint32_t BFPartialCount;
uint16_t BFPredNegTable[16] = {0xFFFF, 0xFFFD, 0xFFFE, 0xFFFB, 0xFFFE, 0xFFFC, 0xFFFD, 0xFFFF};
uint16_t const BFPredTable[8] = {1, 3, 2, 5, 2, 3, 4, 1};

/*
**	The NEW path's routine for (shape flags, line flags), generated from the
**	assembly's NewShapeJumpTable (block = BLIT_ bits of the shape's flags;
**	AllFlagsJumpTable is block 15). Entries 4 and 5 of a block are rewritten
**	by every fading draw, as the assembly patched its table.
*/
enum LineRoutine {
	L_COPY, L_SKIP, L_TRANS, L_GHOST, L_GHOST_T, L_FADING, L_FADING_T, L_SINGLE_FADE, L_SINGLE_FADE_T,
	L_GHOST_FADING, L_GHOST_FADING_T, L_PRED, L_PRED_T, L_PRED_GHOST, L_PRED_GHOST_T, L_PRED_FADING,
	L_PRED_FADING_T, L_PRED_GHOST_FADING, L_PRED_GHOST_FADING_T
};
unsigned char LineTable[16][32] = {
	{L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_GHOST, L_GHOST, L_COPY, L_COPY, L_GHOST, L_GHOST, L_COPY, L_COPY, L_GHOST, L_GHOST, L_COPY, L_COPY, L_GHOST, L_GHOST, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_COPY, L_COPY, L_SINGLE_FADE, L_SINGLE_FADE, L_FADING, L_FADING, L_COPY, L_COPY, L_COPY, L_COPY, L_FADING, L_FADING, L_FADING, L_FADING, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_COPY, L_TRANS, L_SINGLE_FADE, L_SINGLE_FADE_T, L_FADING, L_FADING_T, L_COPY, L_TRANS, L_COPY, L_TRANS, L_FADING, L_FADING_T, L_FADING, L_FADING_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_GHOST, L_GHOST, L_SINGLE_FADE, L_SINGLE_FADE, L_GHOST_FADING, L_GHOST_FADING, L_COPY, L_COPY, L_GHOST, L_GHOST, L_FADING, L_FADING, L_GHOST_FADING, L_GHOST_FADING, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_SINGLE_FADE, L_SINGLE_FADE_T, L_GHOST_FADING, L_GHOST_FADING_T, L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_FADING, L_FADING_T, L_GHOST_FADING, L_GHOST_FADING_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_COPY, L_PRED, L_PRED, L_PRED, L_PRED, L_PRED, L_PRED, L_PRED, L_PRED, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_COPY, L_TRANS, L_PRED, L_PRED_T, L_PRED, L_PRED_T, L_PRED, L_PRED_T, L_PRED, L_PRED_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_GHOST, L_GHOST, L_COPY, L_COPY, L_GHOST, L_GHOST, L_PRED, L_PRED, L_PRED_GHOST, L_PRED_GHOST, L_PRED, L_PRED, L_PRED_GHOST, L_PRED_GHOST, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_PRED, L_PRED_T, L_PRED_GHOST, L_PRED_GHOST_T, L_PRED, L_PRED_T, L_PRED_GHOST, L_PRED_GHOST_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_COPY, L_COPY, L_SINGLE_FADE, L_SINGLE_FADE, L_FADING, L_FADING, L_PRED, L_PRED, L_PRED, L_PRED, L_PRED_FADING, L_PRED_FADING, L_PRED_FADING, L_PRED_FADING, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_COPY, L_TRANS, L_SINGLE_FADE, L_SINGLE_FADE_T, L_FADING, L_FADING_T, L_PRED, L_PRED_T, L_PRED, L_PRED_T, L_PRED_FADING, L_PRED_FADING_T, L_PRED_FADING, L_PRED_FADING_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_COPY, L_GHOST, L_GHOST, L_SINGLE_FADE, L_SINGLE_FADE, L_GHOST_FADING, L_GHOST_FADING, L_PRED, L_PRED, L_PRED_GHOST, L_PRED_GHOST, L_PRED_FADING, L_PRED_FADING, L_PRED_GHOST_FADING, L_PRED_GHOST_FADING, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
	{L_COPY, L_TRANS, L_GHOST, L_GHOST_T, L_SINGLE_FADE, L_SINGLE_FADE_T, L_GHOST_FADING, L_GHOST_FADING_T, L_PRED, L_PRED_T, L_PRED_GHOST, L_PRED_GHOST_T, L_PRED_FADING, L_PRED_FADING_T, L_PRED_GHOST_FADING, L_PRED_GHOST_FADING_T, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP, L_SKIP},
};

struct Effects {
	unsigned char const *	IsTranslucent;
	unsigned char const *	Translucent;		// IsTranslucent + 256
	unsigned char const *	FadingTable;
	uint32_t						FadingNum;
};

inline unsigned char ghost(Effects const & e, uint32_t index, unsigned char pixel, unsigned char dest)
{
	unsigned char t = e.IsTranslucent[index];
	return (t == 0xFF) ? pixel : e.Translucent[((uint32_t)t << 8) + dest];
}

inline unsigned char fade(Effects const & e, unsigned char v)
{
	for (uint32_t n = e.FadingNum; n; n--) v = e.FadingTable[v];
	return v;
}

/*
**	One pixel's turn of the shimmer. True when this pixel shimmers: `v` gets
**	the destination pixel at the current offset (`off`), and the offset
**	steps on. `and_bug` is the Predator+Fading line routines' `and` for `add`.
*/
inline bool pred_step(unsigned char const * d, unsigned char & v, uint16_t & off, bool and_bug = false)
{
	uint32_t count = BFPartialCount + BFPartialPred;
	if (!(count & 0xFF00)) {
		BFPartialCount = count;
		return false;
	}
	BFPartialCount = count & ~0xFF00u;
	uint32_t const old = BFPredOffset;
	uint8_t low = (uint8_t)old;
	low = and_bug ? (uint8_t)(low & 2) : (uint8_t)(low + 2);
	BFPredOffset = (old & ~0xFFu) | (low & PRED_MASK);
	off = (old & 0xFFFFFF00u) ? BFPredNegTable[8 + ((old & 0xFF) >> 1)] : BFPredTable[(old & 0xFF) >> 1];
	v = d[off];
	return true;
}

/*
**	OLD path: one pass over the clipped rectangle with the effect combination
**	(the assembly's sixteen BF_* routines).
*/
void old_draw(Effects const & e, int jtrans, int jghost, int jfade, int jpred,
	unsigned char const * s, unsigned char * d, int32_t w, int32_t h, int32_t sadj, int32_t dadj)
{
	for (int32_t row = 0; row < h; row++) {
		for (int32_t i = 0; i < w; i++, s++, d++) {
			unsigned char px = *s;
			if (jtrans && px == 0) continue;
			unsigned char v = px;
			if (jpred) {
				uint16_t off;
				unsigned char shim;
				bool p = pred_step(d, shim, off);
				if (!jghost && !jfade) {				// BF_Predator / BF_Predator_Trans
					if (p) *d = shim;
					else if (jtrans) *d = px;
					continue;
				}
				if (p) v = shim;
			}
			if (jghost) v = ghost(e, v, v, *d);
			if (jfade) v = fade(e, v);
			*d = v;
		}
		s += sadj;
		d += dadj;
	}
}

/*
**	NEW path: one line through the routine the jump table picked.
*/
void new_line(Effects const & e, int routine, unsigned char const * s, unsigned char * d, int32_t w)
{
	uint32_t eaxhi = 0;				// Predator+Ghost: the high bits left in the pixel register
	for (int32_t i = 0; i < w; i++, s++, d++) {
		unsigned char const px = *s;
		unsigned char v = px, shim = 0;
		uint16_t off = 0;
		bool p;
		switch (routine) {
			case L_COPY:		*d = px; break;
			case L_SKIP:		break;
			case L_TRANS:		if (px) *d = px; break;
			case L_GHOST:		*d = ghost(e, px, px, *d); break;
			case L_GHOST_T:	if (px) *d = ghost(e, px, px, *d); break;
			case L_FADING:
			case L_SINGLE_FADE:	*d = fade(e, px); break;
			case L_FADING_T:
			case L_SINGLE_FADE_T:	if (px) *d = fade(e, px); break;
			case L_GHOST_FADING:	*d = fade(e, ghost(e, px, px, *d)); break;
			case L_GHOST_FADING_T:	if (px) *d = fade(e, ghost(e, px, px, *d)); break;
			case L_PRED:
				if (pred_step(d, shim, off)) *d = shim;
				break;
			case L_PRED_T:
				if (px && pred_step(d, shim, off)) *d = shim;
				break;
			case L_PRED_GHOST:
			case L_PRED_GHOST_T: {
				if (routine == L_PRED_GHOST_T && px == 0) break;
				uint32_t index;
				if (pred_step(d, shim, off)) {
					v = shim;
					eaxhi = off & 0xFF00u;
				}
				index = eaxhi | v;
				unsigned char t = e.IsTranslucent[index];
				if (t == 0xFF) {
					*d = v;
				} else {
					*d = e.Translucent[((uint32_t)t << 8) + *d];
					eaxhi = 0;
				}
				break;
			}
			case L_PRED_FADING:
			case L_PRED_FADING_T:
				if (routine == L_PRED_FADING_T && px == 0) break;
				p = pred_step(d, shim, off, true);
				*d = fade(e, p ? shim : px);
				break;
			case L_PRED_GHOST_FADING:
			case L_PRED_GHOST_FADING_T:
				if (routine == L_PRED_GHOST_FADING_T && px == 0) break;
				pred_step(d, shim, off);
				*d = fade(e, 0);
				break;
		}
	}
}

/*
**	Setup_Shape_Header: classify each line for the NEW path, under these flags
**	and this ghost table, into the header bytes after the ShapeHeader.
*/
void setup_header(int32_t w, int32_t h, unsigned char const * s, unsigned char * header, uint32_t flags,
	unsigned char const * IsTranslucent)
{
	((ShapeHeader *)header)->draw_flags = flags & (F_TRANS | F_FADING | F_PREDATOR | F_GHOST);
	unsigned char * line = header + sizeof(ShapeHeader);
	for (int32_t row = 0; row < h; row++) {
		unsigned bl = 0;
		int32_t trans = 0;
		for (int32_t i = 0; i < w; i++) {
			unsigned char px = *s++;
			if (px == 0 && (flags & F_TRANS)) {
				bl |= BLIT_TRANSPARENT;
				trans++;
				continue;
			}
			if (flags & F_PREDATOR) bl |= BLIT_PREDATOR;
			if ((flags & F_GHOST) && IsTranslucent[px] != 0xFF) bl |= BLIT_GHOST;
			if (flags & F_FADING) bl |= BLIT_FADING;
		}
		unsigned bh = 0;
		if ((bl & BLIT_TRANSPARENT) && trans == w) {
			bh = BLIT_SKIP;
		} else {
			bh = (bl & BLIT_TRANSPARENT) | (bl & (BLIT_PREDATOR | BLIT_GHOST | BLIT_FADING));
		}
		*line++ = (unsigned char)bh;
	}
}

}

/*
**	The optional arguments after `flags`, in the order the assembly read them.
*/
struct WWPortFrameArgs {
	void const *	GhostTable;			// F_GHOST: IsTranslucent, then the translucency rows
	void const *	FadingTable;		// F_FADING
	int				FadingCount;
	int				Predator;			// F_PREDATOR
	int				Partial;				// F_PARTIAL
};

long WWPort_Buffer_Frame_To_Page(int x, int y, int w, int h, void * buffer, WWPortView const & v, int flags,
	WWPortFrameArgs const & a)
{
	if (buffer == NULL) return 0;
	unsigned char const * src = (unsigned char const *)buffer;
	unsigned char * header = NULL;
	bool const big = UseBigShapeBuffer && !UseOldShapeDraw;

	/* A big-buffer shape is passed as its header; the pixels are elsewhere. */
	if (big) {
		header = (unsigned char *)buffer;
		ShapeHeader const * sh = (ShapeHeader const *)header;
		char * base = sh->shape_buffer ? TheaterShapeBufferStart : BigShapeBufferStart;
		src = (unsigned char const *)(base + sh->shape_data);
	}

	if (flags & F_CENTER) {
		x -= w >> 1;
		y -= h >> 1;
	}

	Effects e = {NULL, NULL, NULL, 0};
	int const jtrans = (flags & F_TRANS) != 0;
	int const jghost = (flags & F_GHOST) != 0;
	int const jfade = (flags & F_FADING) != 0;
	int const jpred = (flags & F_PREDATOR) != 0;
	if (jghost) {
		e.IsTranslucent = (unsigned char const *)a.GhostTable;
		e.Translucent = e.IsTranslucent + 256;
	}

	/*
	**	Line classification: build the cache on first draw (then the
	**	all-flags table applies), use it if made for these flags, or draw the
	**	OLD way.
	*/
	bool use_old = true;
	int block = (jpred ? BLIT_PREDATOR : 0) | (jfade ? BLIT_FADING : 0) | (jtrans ? BLIT_TRANSPARENT : 0) | (jghost ? BLIT_GHOST : 0);
	if (big) {
		ShapeHeader * sh = (ShapeHeader *)header;
		uint32_t const mine = (uint32_t)flags & (F_TRANS | F_FADING | F_PREDATOR | F_GHOST);
		if (sh->draw_flags == 0xFFFFFFFFu) {
			setup_header(w, h, src, header, (uint32_t)flags, e.IsTranslucent);
			block = 15;
			use_old = false;
		} else if (sh->draw_flags == mine) {
			use_old = false;
		}
	}

	if (jfade) {
		e.FadingTable = (unsigned char const *)a.FadingTable;
		e.FadingNum = (uint32_t)a.FadingCount & 0x3F;
		LineTable[block][4] = (unsigned char)(e.FadingNum == 1 ? L_SINGLE_FADE : L_FADING);
		LineTable[block][5] = (unsigned char)(e.FadingNum == 1 ? L_SINGLE_FADE_T : L_FADING_T);
	}
	if (jpred) {
		int32_t p = (int32_t)((uint32_t)a.Predator << 1);
		if (p >= 0) {
			BFPredOffset = (uint32_t)p & PRED_MASK;
		} else {
			BFPredOffset = 0xFFFFFF00u | ((0u - (uint32_t)p) & PRED_MASK);
		}
		BFPartialCount = 0;
		BFPartialPred = 0x100;
		for (int i = 0; i < 8; i++) {
			BFPredNegTable[8 + i] = (uint16_t)(BFPredNegTable[i] + (uint32_t)v.Stride());
		}
	}
	if (flags & F_PARTIAL) {
		BFPartialPred = (uint32_t)a.Partial & 0xFF;
	}

	/* clip to the view (the drawing code's outcodes); clipping forces OLD */
	int32_t x0 = x, y0 = y;
	int32_t x1 = (int32_t)((uint32_t)x + (uint32_t)w);
	int32_t y1 = (int32_t)((uint32_t)y + (uint32_t)h);
	int32_t scr_x = 0, scr_y = 0;
	unsigned c0 = WWPort_Outcode(x0, y0, v.Width, v.Height);
	unsigned c1 = WWPort_Outcode(x1, y1, v.Width, v.Height);
	if (c0 & c1) return 0;
	if (c0 | c1) {
		use_old = true;
		if (c0 & 8) {scr_x = -x0; x0 = 0;}
		if (c0 & 2) {scr_y = -y0; y0 = 0;}
		if (c1 & 4) x1 = v.Width;
		if (c1 & 1) y1 = v.Height;
	}

	uint32_t const stride = (uint32_t)v.Stride();
	unsigned char * d = v.Offset + x0 + (int32_t)(stride * (uint32_t)y0);
	int32_t const dadj = (int32_t)(stride + (uint32_t)x0 - (uint32_t)x1);
	int32_t const sadj = (int32_t)((uint32_t)w - (uint32_t)x1 + (uint32_t)x0);
	unsigned char const * s = src + (int32_t)((uint32_t)scr_y * (uint32_t)w + (uint32_t)scr_x);

	if (!use_old) {
		if (w <= 0 || h <= 0) return 0;
		unsigned char const * line = header + sizeof(ShapeHeader);
		for (int32_t row = 0; row < h; row++) {
			new_line(e, LineTable[block][*line++ & BLIT_ALL], s, d, w);
			s += w;
			d += w + dadj;
		}
		return 0;
	}

	/* `sub / jle`: signed comparisons of the operands */
	if (y1 <= y0 || x1 <= x0) return 0;
	old_draw(e, jtrans, jghost, jfade, jpred, s, d, (int32_t)((uint32_t)x1 - (uint32_t)x0),
		(int32_t)((uint32_t)y1 - (uint32_t)y0), sadj, dadj);
	return 0;
}

/*
**	USES eax: the assembly's "return value" was the caller's own EAX; no
**	caller reads it.
*/
extern "C" long __cdecl Buffer_Frame_To_Page(int x, int y, int w, int h, void * Buffer, GraphicViewPortClass & view, int flags, ...)
{
	WWPortFrameArgs a = {NULL, NULL, 0, 0, 0};
	va_list ap;
	va_start(ap, flags);
	if (flags & F_GHOST) a.GhostTable = va_arg(ap, void const *);
	if (flags & F_FADING) {
		a.FadingTable = va_arg(ap, void const *);
		a.FadingCount = va_arg(ap, int);
	}
	if (flags & F_PREDATOR) a.Predator = va_arg(ap, int);
	if (flags & F_PARTIAL) a.Partial = va_arg(ap, int);
	va_end(ap);
	return WWPort_Buffer_Frame_To_Page(x, y, w, h, Buffer, WWPort_View_Of(&view), flags, a);
}
