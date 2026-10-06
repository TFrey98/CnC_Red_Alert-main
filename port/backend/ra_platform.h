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

/*
**	A file read failed. Ask the player whether to try again.
**
**	Returns RA_DISK_ERROR_RETRY or RA_DISK_ERROR_CANCEL. Blocks until answered.
**	`error_code` is the Win32-style code from GetLastError() (the compat layer
**	maps errno onto those); it is shown to the player and otherwise ignored.
**
**	This restores the original contract in CODE/CCFILE.CPP's header comment: on
**	a retryable error the game waits and retries; the alternative exits the game.
**	The engine owns what Cancel means -- this function only asks.
*/
enum { RA_DISK_ERROR_CANCEL = 0, RA_DISK_ERROR_RETRY = 1 };
int RA_Platform_Disk_Error(const char * filename, int error_code);

/*
**	The application and its one window. RA_Platform_Init makes this a regular
**	foreground app (Dock icon, menu bar with Quit); call it before anything else
**	here. It is safe to call more than once.
*/
void RA_Platform_Init(void);
void RA_Display_Show(RA_Display * d);

/*
**	The engine's framebuffer changed size (DirectDraw SetDisplayMode). The window
**	keeps its size; the image is rescaled to fit.
*/
void RA_Display_Resize(RA_Display * d, int width, int height);

/*
**	Where the game data lives. The engine side (port/compat/win32_main.cpp)
**	decides where to look and in what order; these only remember and ask.
**	Paths are UTF-8.
**
**	RA_Platform_Saved_Data_Folder fills `out` with the folder saved last time
**	and returns 1, or returns 0 if none was saved. RA_Platform_Save_Data_Folder
**	saves one for next launch. RA_Platform_Choose_Data_Folder shows a folder
**	picker headed by `message` and returns 1 with the choice in `out`, or 0 if
**	the player cancelled. RA_Platform_Option_Key_Down reports whether Option is
**	held right now (held at launch: choose the folder again).
*/
int  RA_Platform_Saved_Data_Folder(char * out, int size);
void RA_Platform_Save_Data_Folder(const char * path);
int  RA_Platform_Choose_Data_Folder(char * out, int size, const char * message);
int  RA_Platform_Option_Key_Down(void);

/*
**	Input, as plain events. Keys carry WINDOWS virtual-key codes (VK_*): the
**	backend owns the mapping from Mac key codes, so the engine sees exactly the
**	key messages it was written for. Mouse positions are in framebuffer pixels
**	(the engine's own coordinates), not window points.
*/
enum {
	RA_EV_NONE = 0,
	RA_EV_KEY_DOWN,			/* vk, repeat */
	RA_EV_KEY_UP,				/* vk */
	RA_EV_MOUSE_MOVE,			/* x, y */
	RA_EV_BUTTON_DOWN,		/* button (0 left, 1 right, 2 middle), x, y */
	RA_EV_BUTTON_UP,			/* button, x, y */
	RA_EV_ACTIVATE,			/* the app came to the front */
	RA_EV_DEACTIVATE,			/* the app went to the back */
	RA_EV_QUIT					/* window closed, or Quit chosen */
};
typedef struct RA_Event {
	int type;
	int vk;
	int repeat;
	int button;
	int x, y;
} RA_Event;

/*
**	Runs the Cocoa event loop just long enough to collect what is pending, then
**	hands back one event. Returns 1 with *ev filled, or 0 if there is none.
**	Main thread only, like all of Cocoa.
*/
int  RA_Platform_Poll_Event(RA_Event * ev);

/*
**	Blocks in the Cocoa event loop until an event arrives or `milliseconds`
**	pass. For GetMessage, which waits.
*/
void RA_Platform_Wait_Event(int milliseconds);

/*
**	The last known pointer position in framebuffer pixels. Safe from any thread
**	(the mouse timer reads it).
*/
void RA_Platform_Mouse_Position(int * x, int * y);

/*
**	Whether the Mac's own pointer shows over the window. The engine draws its
**	own cursor, so it normally hides this one.
*/
void RA_Platform_Set_Cursor_Visible(int visible);

/*
**	A message box. `buttons`: RA_MB_OK, RA_MB_OKCANCEL or RA_MB_YESNO; `warning`
**	nonzero for the caution style. Returns the button chosen (RA_ID_*). Blocks.
*/
enum {RA_MB_OK = 0, RA_MB_OKCANCEL = 1, RA_MB_YESNO = 2};
enum {RA_ID_OK = 1, RA_ID_CANCEL = 2, RA_ID_YES = 6, RA_ID_NO = 7};
int RA_Platform_Message_Box(const char * text, const char * caption, int buttons, int warning);

/*
**	Audio output. The backend owns the device; the engine side (DirectSound
**	emulation, port/compat/win32_dsound.cpp) does all the mixing. `render` is
**	called on CoreAudio's real-time thread to fill `frames` frames of
**	interleaved stereo float samples (-1..1). It must not block for long.
**
**	Returns 1 and the device's sample rate in *rate, or 0 if there is no output.
*/
typedef void (*RA_Audio_Render)(float * stereo, int frames, void * user);
int  RA_Audio_Start(RA_Audio_Render render, void * user, int * rate);
void RA_Audio_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* RA_PLATFORM_H */
