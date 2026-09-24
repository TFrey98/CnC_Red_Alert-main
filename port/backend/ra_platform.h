/*
**	ra_platform.h -- the boundary between the Red Alert engine and the native
**	macOS backend.
**
**	THIS HEADER IS THE CONTRACT, AND ITS CONSTRAINT IS ABSOLUTE:
**	plain C types only. No Win32 types, no engine headers, no C++.
**
**	The reason is not style. The engine's Win32 shim and Cocoa/Metal headers
**	CANNOT coexist in one translation unit, and no include ordering fixes it:
**
**	  - port/compat/windows.h has `typedef int BOOL`. <objc/objc.h> has
**	    `typedef bool BOOL`. That is a hard typedef redefinition error.
**	  - windows.h defines min/max as function-like macros. Metal's own
**	    MTLAccelerationStructureTypes.h calls `min(a, b, c)` with three
**	    arguments, which the macro mangles.
**
**	So the split is enforced structurally. Engine-side .CPP files are built with
**	-DWIN32 and -include wwcompat.h and may include this header. Backend-side
**	.mm files are built with NEITHER, include Cocoa/Metal freely, and may
**	include this header. Nothing includes both sides. This file is the only
**	thing they share, which is why it must stay this austere.
*/
#ifndef RA_PLATFORM_H
#define RA_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RA_Display RA_Display;

/*
**	Creates the window, Metal device and swapchain. `width`/`height` are the
**	engine's framebuffer size in palette indices (320x200, 640x400), not points
**	-- scaling to the window is the backend's business.
**
**	Returns NULL on failure.
*/
RA_Display * RA_Display_Create(int width, int height, const char * title);
void         RA_Display_Destroy(RA_Display * d);

/*
**	`rgba` is 256 entries x 4 bytes (R,G,B,unused), 0-255 per channel.
**
**	Note the engine's palettes are VGA 6-bit (0-63) and must be scaled up before
**	they get here; the backend does not guess at that, because the engine also
**	uses genuinely 8-bit palettes in places and silently rescaling both would
**	corrupt one of them.
**
**	Cheap by design: a palette change is a 1KB upload, not a re-expansion of the
**	framebuffer. The engine mutates the palette directly for fades, so this is
**	called far more often than one might expect.
*/
void RA_Display_SetPalette(RA_Display * d, const unsigned char * rgba);

/*
**	`indices` is width*height bytes, one palette index per pixel. Uploaded as
**	r8uint and resolved against the palette in the fragment shader, so the
**	8-bit paletted model survives all the way to the GPU.
*/
void RA_Display_Present(RA_Display * d, const unsigned char * indices);

#ifdef __cplusplus
}
#endif

#endif /* RA_PLATFORM_H */
