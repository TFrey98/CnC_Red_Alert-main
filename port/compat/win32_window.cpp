/*
**	win32_window.cpp -- the Win32 window, message and input calls the engine
**	makes, implemented over the native backend (port/backend/ra_platform.h).
**
**	Engine side: built with the engine's flags (-DWIN32, -include wwcompat.h).
**	It never sees Cocoa; it talks to the backend only through ra_platform.h.
**
**	The engine's own platform code (WINSTUB.CPP, KEY.CPP, STARTUP.CPP) is kept
**	as Westwood wrote it. It registers a window class, creates a window, and
**	runs the usual PeekMessage / GetMessage / DispatchMessage loop into its
**	window procedure. This file makes that work:
**
**	  * One window. CreateWindowEx opens the Mac window (RA_Display) and the
**	    DirectDraw emulation presents into it (WWPort_Main_Display).
**	  * A message queue. Backend events (key, mouse, focus, quit) become the
**	    WM_ messages Windows would have sent, with the same wParam/lParam
**	    encodings; PostMessage adds the engine's own. Pumping happens inside
**	    PeekMessage/GetMessage on the main thread, as Cocoa requires.
**	  * Key state. GetAsyncKeyState/GetKeyState answer from the keys seen so
**	    far; ToAscii/VkKeyScan translate with a US layout, matching the
**	    backend's key mapping (ra_input.mm).
**	  * The Mac pointer is hidden over the window while the engine has it
**	    hidden (ShowCursor), since the engine draws its own cursor.
**	  * Scripted input, for testing UI flows without a person at the window:
**	    RA_INPUT_SCRIPT=<file> (see Script below).
*/

#include "windows.h"
#include "ra_platform.h"
#include "win32_internal.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

/* ---------------------------------------------------------------- windows */

namespace {

struct Window {
	WNDPROC			proc;
	RA_Display *	display;
	bool				destroyed;
};

std::map<std::string, WNDPROC> Classes;
Window * MainWindow = NULL;

inline Window * W(HWND h) {return (Window *)h;}

}

RA_Display * WWPort_Main_Display(void)
{
	return MainWindow ? MainWindow->display : NULL;
}

ATOM RegisterClassA(const WNDCLASSA * wc)
{
	if (wc == NULL || wc->lpszClassName == NULL) return 0;
	Classes[wc->lpszClassName] = wc->lpfnWndProc;
	return 1;
}

/*
**	The window opens with a 640 x 400 framebuffer; DirectDraw's SetDisplayMode
**	resizes it to whatever the game asks for. Position and size are the Mac
**	window's business, so the arguments for them are ignored.
*/
HWND CreateWindowExA(DWORD exstyle, LPCSTR classname, LPCSTR title, DWORD style, int x, int y, int w, int h,
	HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
	(void)exstyle; (void)style; (void)x; (void)y; (void)w; (void)h; (void)parent; (void)menu; (void)instance;
	std::map<std::string, WNDPROC>::iterator c = Classes.find(classname ? classname : "");
	if (c == Classes.end()) return NULL;
	Window * win = new Window;
	win->proc = c->second;
	win->destroyed = false;
	win->display = RA_Display_Create(640, 400, title);
	if (win->display == NULL) {
		delete win;
		return NULL;
	}
	if (MainWindow == NULL) MainWindow = win;
	if (win->proc) win->proc((HWND)win, WM_CREATE, 0, (LPARAM)param);
	return (HWND)win;
}

BOOL ShowWindow(HWND wnd, int cmdshow)
{
	if (wnd == NULL) return FALSE;
	if (cmdshow != SW_HIDE) RA_Display_Show(W(wnd)->display);
	return TRUE;
}

BOOL UpdateWindow(HWND wnd) {return wnd != NULL;}
HWND SetFocus(HWND wnd) {HWND old = (HWND)MainWindow; if (wnd) RA_Display_Show(W(wnd)->display); return old;}
HWND GetFocus(void) {return (HWND)MainWindow;}
BOOL SetForegroundWindow(HWND wnd) {if (wnd) RA_Display_Show(W(wnd)->display); return wnd != NULL;}
HWND FindWindowA(LPCSTR classname, LPCSTR windowname) {(void)classname; (void)windowname; return NULL;}

/*
**	The "screen" is the game's framebuffer: the window is its whole world, as
**	the original's fullscreen popup window was.
*/
int GetSystemMetrics(int index)
{
	switch (index) {
		case SM_CXSCREEN: return 640;
		case SM_CYSCREEN: return 480;
		default: return 0;
	}
}

BOOL DestroyWindow(HWND wnd)
{
	Window * win = W(wnd);
	if (win == NULL || win->destroyed) return FALSE;
	win->destroyed = true;
	if (win->proc) win->proc(wnd, WM_DESTROY, 0, 0);
	return TRUE;
}

/*
**	DefWindowProc: the only default behaviour the game relies on is that
**	WM_CLOSE destroys the window (which sends WM_DESTROY, the game's shutdown).
*/
LRESULT DefWindowProcA(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	(void)wparam; (void)lparam;
	if (msg == WM_CLOSE) DestroyWindow(wnd);
	return 0;
}

LRESULT SendMessageA(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	Window * win = W(wnd);
	if (win == NULL || win->proc == NULL) return 0;
	return win->proc(wnd, msg, wparam, lparam);
}

UINT RegisterWindowMessageA(LPCSTR name)
{
	static std::map<std::string, UINT> ids;
	static UINT next = 0xC000;			// Windows' range for registered messages
	std::map<std::string, UINT>::iterator i = ids.find(name ? name : "");
	if (i != ids.end()) return i->second;
	return ids[name ? name : ""] = next++;
}

/*
**	Dialog boxes come from .RC resources the Mac build does not have. The only
**	caller, WINSTUB.CPP's Window_Dialog_Box, is itself never called; this fails
**	as Windows does for a missing template.
*/
INT_PTR DialogBoxA(HINSTANCE instance, LPCSTR templ, HWND owner, DLGPROC proc)
{
	(void)instance; (void)templ; (void)owner; (void)proc;
	return -1;
}

HICON LoadIconA(HINSTANCE instance, LPCSTR name) {(void)instance; (void)name; return (HICON)1;}
HCURSOR LoadCursorA(HINSTANCE instance, LPCSTR name) {(void)instance; (void)name; return (HCURSOR)1;}

/* ------------------------------------------------------------ key state */

namespace {

/*
**	Bit 7: down. Bit 0: toggled (Caps Lock, Num Lock). Written on the main
**	thread as events are translated; read from any thread.
*/
std::atomic<unsigned char> Keys[256];
unsigned MouseButtons = 0;			// MK_ bits, main thread

void key_event(int vk, bool down)
{
	vk &= 0xFF;
	unsigned char k = Keys[vk];
	if (down && !(k & 0x80)) k ^= 0x01;			// toggles on each fresh press
	k = down ? (unsigned char)(k | 0x80) : (unsigned char)(k & ~0x80);
	Keys[vk] = k;
}

}

SHORT GetAsyncKeyState(int vk)
{
	return (Keys[vk & 0xFF] & 0x80) ? (SHORT)0x8000 : 0;
}

SHORT GetKeyState(int vk)
{
	unsigned char k = Keys[vk & 0xFF];
	return (SHORT)(((k & 0x80) ? 0x8000 : 0) | (k & 0x01));
}

/*
**	MAPVK_VK_TO_VSC: the engine only passes the result back into ToAscii, which
**	ignores it, so any stable nonzero value serves; the key code itself is used.
*/
UINT MapVirtualKeyA(UINT code, UINT maptype)
{
	(void)maptype;
	return code & 0xFF;
}

/* ------------------------------------------------- US keyboard layout */

namespace {

struct KeyChar {unsigned char vk; char plain, shifted;};

/* Every key that types a character, with its unshifted and shifted forms. */
KeyChar const Layout[] = {
	{0x30, '0', ')'}, {0x31, '1', '!'}, {0x32, '2', '@'}, {0x33, '3', '#'}, {0x34, '4', '$'},
	{0x35, '5', '%'}, {0x36, '6', '^'}, {0x37, '7', '&'}, {0x38, '8', '*'}, {0x39, '9', '('},
	{0xBA, ';', ':'}, {0xBB, '=', '+'}, {0xBC, ',', '<'}, {0xBD, '-', '_'}, {0xBE, '.', '>'},
	{0xBF, '/', '?'}, {0xC0, '`', '~'}, {0xDB, '[', '{'}, {0xDC, '\\', '|'}, {0xDD, ']', '}'},
	{0xDE, '\'', '"'},
	{0x20, ' ', ' '}, {0x0D, '\r', '\r'}, {0x08, '\b', '\b'}, {0x09, '\t', '\t'}, {0x1B, 0x1B, 0x1B},
	{0x60, '0', '0'}, {0x61, '1', '1'}, {0x62, '2', '2'}, {0x63, '3', '3'}, {0x64, '4', '4'},
	{0x65, '5', '5'}, {0x66, '6', '6'}, {0x67, '7', '7'}, {0x68, '8', '8'}, {0x69, '9', '9'},
	{0x6A, '*', '*'}, {0x6B, '+', '+'}, {0x6D, '-', '-'}, {0x6E, '.', '.'}, {0x6F, '/', '/'},
};

}

/*
**	ToAscii with the state of Shift, Caps Lock and Ctrl taken from `keystate`,
**	as Windows does: Ctrl+letter gives the control character. Returns 1 and the
**	character, or 0 for a key that types nothing.
*/
int ToAscii(UINT vk, UINT scancode, const BYTE * keystate, LPWORD out, UINT flags)
{
	(void)scancode; (void)flags;
	vk &= 0xFF;
	bool shift = keystate && (keystate[0x10] & 0x80);
	bool caps = keystate && (keystate[0x14] & 0x01);
	bool ctrl = keystate && (keystate[0x11] & 0x80);
	int ch = -1;
	if (vk >= 'A' && vk <= 'Z') {
		ch = (shift != caps) ? (int)vk : (int)vk + ('a' - 'A');
		if (ctrl) ch = vk & 0x1F;
	} else {
		for (KeyChar const & k : Layout) {
			if (k.vk == vk) {ch = (unsigned char)(shift ? k.shifted : k.plain); break;}
		}
	}
	if (ch < 0) return 0;
	if (out) *out = (WORD)ch;
	return 1;
}

/*
**	The key, and the shift state (high byte: 1 Shift, 2 Ctrl), that types `c`;
**	-1 if no key does. KEYBOARD.CPP builds its character tables from this.
*/
SHORT VkKeyScanA(char c)
{
	unsigned char u = (unsigned char)c;
	if (u >= 'a' && u <= 'z') return (SHORT)(u - 'a' + 'A');
	if (u >= 'A' && u <= 'Z') return (SHORT)(0x100 | u);
	if (u >= 1 && u <= 26) return (SHORT)(0x200 | (u + 'A' - 1));
	for (KeyChar const & k : Layout) {
		if (k.vk >= 0x60 && k.vk <= 0x6F) continue;			// prefer the main keyboard
		if ((unsigned char)k.plain == u) return (SHORT)k.vk;
		if ((unsigned char)k.shifted == u) return (SHORT)(0x100 | k.vk);
	}
	return -1;
}

/* --------------------------------------------------------------- cursor */

namespace {
int CursorCount = 0;				// Windows' display counter: shown while >= 0
POINT LastMouse = {0, 0};
}

int ShowCursor(BOOL show)
{
	CursorCount += show ? 1 : -1;
	RA_Platform_Set_Cursor_Visible(CursorCount >= 0);
	return CursorCount;
}

HCURSOR SetCursor(HCURSOR cursor) {(void)cursor; return NULL;}

/*
**	Confining the pointer is not something a Mac window may do; the backend
**	clamps reported positions to the framebuffer instead.
*/
BOOL ClipCursor(const RECT * rect) {(void)rect; return TRUE;}

namespace { bool Scripted(void); }	// scripted input, below

BOOL GetCursorPos(LPPOINT point)
{
	if (point == NULL) return FALSE;
	if (Scripted()) {				// the script owns the pointer
		*point = LastMouse;
		return TRUE;
	}
	int x, y;
	RA_Platform_Mouse_Position(&x, &y);
	point->x = x;
	point->y = y;
	return TRUE;
}

/* The window's client area is the whole "screen" (see GetSystemMetrics). */
BOOL ScreenToClient(HWND wnd, LPPOINT point) {(void)wnd; return point != NULL;}
BOOL ClientToScreen(HWND wnd, LPPOINT point) {(void)wnd; return point != NULL;}

/* --------------------------------------------------------------- messages */

namespace {

std::mutex QueueLock;				// PostMessage may come from a timer thread
std::deque<MSG> Queue;

void post(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
	MSG m;
	memset(&m, 0, sizeof(m));
	m.hwnd = wnd;
	m.message = msg;
	m.wParam = w;
	m.lParam = l;
	m.time = GetTickCount();
	m.pt = LastMouse;
	std::lock_guard<std::mutex> g(QueueLock);
	Queue.push_back(m);
}

inline LPARAM xy(int x, int y) {return (LPARAM)(((unsigned)y & 0xFFFF) << 16 | ((unsigned)x & 0xFFFF));}

/*
**	One backend event -> the message(s) Windows would have queued for it.
**	Key lParam: repeat count 1, bit 30 "was already down", bit 31 "released";
**	Alt and F10 come as WM_SYSKEY*, as on Windows.
*/
void translate(RA_Event const & e)
{
	HWND wnd = (HWND)MainWindow;
	switch (e.type) {
		case RA_EV_KEY_DOWN: {
			bool sys = (Keys[0x12] & 0x80) || e.vk == 0x12 || e.vk == 0x79;
			key_event(e.vk, true);
			post(wnd, sys ? WM_SYSKEYDOWN : WM_KEYDOWN, (WPARAM)e.vk, 1 | (e.repeat ? (1L << 30) : 0));
			break;
		}
		case RA_EV_KEY_UP: {
			bool sys = (Keys[0x12] & 0x80) || e.vk == 0x12 || e.vk == 0x79;
			key_event(e.vk, false);
			post(wnd, sys ? WM_SYSKEYUP : WM_KEYUP, (WPARAM)e.vk, 1 | (1L << 30) | (1L << 31));
			break;
		}
		case RA_EV_MOUSE_MOVE:
			LastMouse.x = e.x; LastMouse.y = e.y;
			post(wnd, WM_MOUSEMOVE, MouseButtons, xy(e.x, e.y));
			break;
		case RA_EV_BUTTON_DOWN:
		case RA_EV_BUTTON_UP: {
			static UINT const down[3] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN};
			static UINT const up[3] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP};
			static int const vk[3] = {0x01, 0x02, 0x04};			// VK_LBUTTON, VK_RBUTTON, VK_MBUTTON
			static unsigned const mk[3] = {0x0001, 0x0002, 0x0010};	// MK_LBUTTON, MK_RBUTTON, MK_MBUTTON
			int b = e.button < 0 || e.button > 2 ? 0 : e.button;
			bool pressed = e.type == RA_EV_BUTTON_DOWN;
			LastMouse.x = e.x; LastMouse.y = e.y;
			key_event(vk[b], pressed);
			MouseButtons = pressed ? (MouseButtons | mk[b]) : (MouseButtons & ~mk[b]);
			post(wnd, pressed ? down[b] : up[b], MouseButtons, xy(e.x, e.y));
			break;
		}
		case RA_EV_ACTIVATE:
		case RA_EV_DEACTIVATE:
			if (e.type == RA_EV_DEACTIVATE) {			// keys held when focus left are released
				for (int i = 0; i < 256; i++) Keys[i] = (unsigned char)(Keys[i] & 0x01);
				MouseButtons = 0;
			}
			post(wnd, WM_ACTIVATEAPP, e.type == RA_EV_ACTIVATE, 0);
			break;
		case RA_EV_QUIT:
			post(wnd, WM_CLOSE, 0, 0);
			break;
	}
}

/*
**	Scripted input. RA_INPUT_SCRIPT names a text file of timed events, fed in
**	through translate() exactly as backend events are; while it is set, real
**	input and focus events are ignored and the pointer position is the script's.
**	One event per line, time in seconds from the first message pump:
**
**	    12.5  move  320 200          pointer to (x, y), framebuffer pixels
**	    14    click 600 180 [right]  move, press, release 50 ms later
**	    15    key   0x1B             press and release a virtual key
**	    20    deactivate             the app loses focus (clicked elsewhere)
**	    25    activate               the app gets focus back
**	    90    quit                   close the window
**
**	A script starts with an implicit activate at time 0: on Windows the window
**	was always activated when it opened, and the game waits for that
**	(INIT.CPP), but a test window opened behind other apps never gets it.
**
**	Blank lines and lines starting with # are ignored.
*/
struct ScriptEvent {double at; RA_Event e;};
std::vector<ScriptEvent> Script;
size_t ScriptNext = 0;
DWORD ScriptStart = 0;
int ScriptState = -1;				// -1 unread, 0 none, 1 active

void load_script(void)
{
	ScriptState = 0;
	char const * path = getenv("RA_INPUT_SCRIPT");
	if (path == NULL || path[0] == 0) return;
	FILE * f = fopen(path, "r");
	if (f == NULL) {fprintf(stderr, "RA_INPUT_SCRIPT: cannot open %s\n", path); return;}
	char line[256];
	while (fgets(line, sizeof(line), f)) {
		double at; char cmd[16] = {0}; char a[16] = {0}, b[16] = {0}, c[16] = {0};
		if (line[0] == '#' || sscanf(line, "%lf %15s %15s %15s %15s", &at, cmd, a, b, c) < 2) continue;
		RA_Event e; memset(&e, 0, sizeof(e));
		e.x = (int)strtol(a, NULL, 0); e.y = (int)strtol(b, NULL, 0);
		if (strcmp(cmd, "move") == 0 || strcmp(cmd, "click") == 0) {
			e.type = RA_EV_MOUSE_MOVE; Script.push_back({at, e});
			if (cmd[0] == 'c') {
				e.button = strcmp(c, "right") == 0 ? 1 : 0;
				e.type = RA_EV_BUTTON_DOWN; Script.push_back({at, e});
				e.type = RA_EV_BUTTON_UP; Script.push_back({at + 0.05, e});
			}
		} else if (strcmp(cmd, "key") == 0) {
			e.vk = (int)strtol(a, NULL, 0); e.x = LastMouse.x; e.y = LastMouse.y;
			e.type = RA_EV_KEY_DOWN; Script.push_back({at, e});
			e.type = RA_EV_KEY_UP; Script.push_back({at + 0.05, e});
		} else if (strcmp(cmd, "quit") == 0) {
			e.type = RA_EV_QUIT; Script.push_back({at, e});
		} else if (strcmp(cmd, "activate") == 0 || strcmp(cmd, "deactivate") == 0) {
			e.type = cmd[0] == 'a' ? RA_EV_ACTIVATE : RA_EV_DEACTIVATE; Script.push_back({at, e});
		}
	}
	fclose(f);
	{RA_Event e; memset(&e, 0, sizeof(e)); e.type = RA_EV_ACTIVATE; Script.insert(Script.begin(), ScriptEvent{0, e});}
	std::stable_sort(Script.begin(), Script.end(), [](ScriptEvent const & l, ScriptEvent const & r) {return l.at < r.at;});
	ScriptStart = GetTickCount();
	ScriptState = 1;
	fprintf(stderr, "RA_INPUT_SCRIPT: %zu events from %s\n", Script.size(), path);
}

bool Scripted(void)
{
	if (ScriptState < 0) load_script();
	return ScriptState == 1;
}

void pump(void)
{
	RA_Event e;
	bool const scripted = Scripted();
	while (RA_Platform_Poll_Event(&e)) {
		if (!(scripted && e.type != RA_EV_QUIT)) translate(e);	// a script owns input and focus: the test is unaffected by using the Mac meanwhile
	}
	if (scripted) {
		double now = (GetTickCount() - ScriptStart) / 1000.0;
		while (ScriptNext < Script.size() && Script[ScriptNext].at <= now) {
			translate(Script[ScriptNext++].e);
		}
	}
	WWPort_Display_Pump();
}

bool matches(MSG const & m, HWND wnd, UINT lo, UINT hi)
{
	if (wnd != NULL && m.hwnd != wnd) return false;
	if (lo == 0 && hi == 0) return true;
	return m.message >= lo && m.message <= hi;
}

bool take(LPMSG out, HWND wnd, UINT lo, UINT hi, bool remove)
{
	std::lock_guard<std::mutex> g(QueueLock);
	for (std::deque<MSG>::iterator i = Queue.begin(); i != Queue.end(); ++i) {
		if (matches(*i, wnd, lo, hi)) {
			if (out) *out = *i;
			if (remove) Queue.erase(i);
			return true;
		}
	}
	return false;
}

}

BOOL PostMessageA(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	post(wnd, msg, wparam, lparam);
	return TRUE;
}

void PostQuitMessage(int code)
{
	post(NULL, WM_QUIT, (WPARAM)code, 0);
}

BOOL PeekMessageA(LPMSG msg, HWND wnd, UINT lo, UINT hi, UINT remove)
{
	pump();
	return take(msg, wnd, lo, hi, (remove & PM_REMOVE) != 0) ? TRUE : FALSE;
}

/*
**	Waits for a message, then returns FALSE for WM_QUIT and TRUE otherwise.
*/
BOOL GetMessageA(LPMSG msg, HWND wnd, UINT lo, UINT hi)
{
	for (;;) {
		pump();
		if (take(msg, wnd, lo, hi, true)) return msg->message != WM_QUIT;
		RA_Platform_Wait_Event(10);
	}
}

/*
**	Windows would add a WM_CHAR after each WM_KEYDOWN here; the engine reads
**	characters through ToAscii instead and ignores WM_CHAR, so none are made.
*/
BOOL TranslateMessage(const MSG * msg) {(void)msg; return FALSE;}

LRESULT DispatchMessageA(const MSG * msg)
{
	if (msg == NULL) return 0;
	Window * win = W(msg->hwnd ? msg->hwnd : (HWND)MainWindow);
	if (win == NULL || win->proc == NULL || win->destroyed) return 0;
	return win->proc((HWND)win, msg->message, msg->wParam, msg->lParam);
}
