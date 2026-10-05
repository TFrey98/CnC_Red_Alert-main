/*
**	ww_view.h -- PORT-CREATED. The view-port fields Westwood's drawing assembly
**	read, as a plain struct, for the C translations of that assembly.
**
**	The assembly read GraphicViewPortClass through a 32-bit struct overlay
**	(GBUFFER.INC: GVPOffset, GVPWidth, GVPHeight, GVPXAdd, GVPXPos, GVPYPos,
**	GVPPitch). On arm64 the C++ object is laid out differently -- Offset and
**	Pitch are 64-bit -- so the translations never overlay it: each entry point
**	copies the fields out through the class's own inline getters, and the
**	translated logic works on this struct. That also lets port/tests exercise
**	the translations without constructing a GraphicViewPortClass.
*/
#ifndef WW_VIEW_H
#define WW_VIEW_H

#include <stdint.h>

struct WWPortView {
	unsigned char *	Offset;		// first byte of the view
	int32_t				Width;
	int32_t				Height;
	int32_t				XAdd;			// bytes from the end of one line to the next...
	int32_t				Pitch;		// ...plus this (the DirectDraw surface's extra pitch)
	int32_t				XPos;
	int32_t				YPos;

	/* Bytes from the start of one line to the start of the next. */
	int32_t Stride(void) const {return Width + XAdd + Pitch;}
};

/*
**	The clip outcode the drawing assembly (REMAP, TOBUFF, TOPAGE, ...) builds
**	with four `shld` sign-bit shifts and an `xor cl,5`:
**
**	  8  x < 0                      2  y < 0
**	  4  x - (w + 1) >= 0           1  y - (h + 1) >= 0
**
**	"Right of" and "below" are the sign bit of a wrapping 32-bit subtraction,
**	not a true comparison; this reproduces that exactly.
*/
inline unsigned WWPort_Outcode(int32_t x, int32_t y, int32_t w, int32_t h)
{
	return ((uint32_t)x >> 31) << 3
	     | (((uint32_t)x - (uint32_t)(w + 1)) >> 31 ^ 1) << 2
	     | ((uint32_t)y >> 31) << 1
	     | (((uint32_t)y - (uint32_t)(h + 1)) >> 31 ^ 1);
}

/*
**	Works for any class with GraphicViewPortClass's getters; a template so this
**	header does not need gbuffer.h. The getters are not const, hence the cast.
*/
template <class GVP>
inline WWPortView WWPort_View_Of(GVP const * vp)
{
	GVP * v = const_cast<GVP *>(vp);
	WWPortView r;
	r.Offset = (unsigned char *)v->Get_Offset();
	r.Width = v->Get_Width();
	r.Height = v->Get_Height();
	r.XAdd = v->Get_XAdd();
	r.Pitch = v->Get_Pitch();
	r.XPos = v->Get_XPos();
	r.YPos = v->Get_YPos();
	return r;
}

#endif
