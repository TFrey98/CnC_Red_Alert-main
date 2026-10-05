/*
**	ra_input.mm -- the application, its event loop, and input for the engine.
**
**	Built WITHOUT -DWIN32 and WITHOUT -include wwcompat.h (see ra_platform.h).
**
**	The engine was written for a Win32 message loop: it calls PeekMessage /
**	GetMessage from its own main loop, and its window procedure handles
**	WM_KEYDOWN, WM_LBUTTONDOWN and so on. The compat layer
**	(port/compat/win32_window.cpp) provides those calls; this file feeds them.
**	Cocoa delivers NSEvents to the view (ra_metal.mm), which turns them into
**	RA_Events here; RA_Platform_Poll_Event runs the Cocoa loop just long enough
**	to collect what is pending and hands them over one at a time. Everything
**	runs on the main thread, as Cocoa requires -- the engine's loop IS the
**	main thread's loop.
*/
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>		/* kVK_* key codes */
#include <atomic>
#include <deque>

#include "ra_platform.h"
#include "ra_internal.h"

/* ------------------------------------------------------------------ queue */

static std::deque<RA_Event> Queue;					/* main thread only */
static std::atomic<int> MouseX(0), MouseY(0);	/* read by the mouse timer thread */

void RA_Input_Push(RA_Event const & e)
{
	if (e.type == RA_EV_MOUSE_MOVE || e.type == RA_EV_BUTTON_DOWN || e.type == RA_EV_BUTTON_UP) {
		MouseX = e.x;
		MouseY = e.y;
	}
	Queue.push_back(e);
}

void RA_Platform_Mouse_Position(int * x, int * y)
{
	if (x) *x = MouseX;
	if (y) *y = MouseY;
}

/* -------------------------------------------------------------- app setup */

@interface RAAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation RAAppDelegate
/*
**	Quit (Cmd-Q, the menu, logging out) is handed to the game as an event
**	rather than letting Cocoa exit underneath it: the game shuts down through
**	its own WM_DESTROY path.
*/
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	RA_Event e = {RA_EV_QUIT, 0, 0, 0, 0, 0};
	RA_Input_Push(e);
	return NSTerminateCancel;
}
- (void)applicationDidBecomeActive:(NSNotification *)n
{
	RA_Event e = {RA_EV_ACTIVATE, 0, 0, 0, 0, 0};
	RA_Input_Push(e);
}
- (void)applicationDidResignActive:(NSNotification *)n
{
	RA_Event e = {RA_EV_DEACTIVATE, 0, 0, 0, 0, 0};
	RA_Input_Push(e);
}
@end

static RAAppDelegate * AppDelegate = nil;

void RA_Platform_Init(void)
{
	if (AppDelegate != nil) return;
	[NSApplication sharedApplication];
	[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

	/* A minimal menu bar: the application menu with Quit. */
	NSMenu * bar = [[NSMenu alloc] init];
	NSMenuItem * appItem = [[NSMenuItem alloc] init];
	[bar addItem:appItem];
	NSMenu * appMenu = [[NSMenu alloc] init];
	NSString * name = [[NSProcessInfo processInfo] processName];
	[appMenu addItemWithTitle:[@"Quit " stringByAppendingString:name]
	                   action:@selector(terminate:)
	            keyEquivalent:@"q"];
	[appItem setSubmenu:appMenu];
	[NSApp setMainMenu:bar];

	AppDelegate = [[RAAppDelegate alloc] init];
	[NSApp setDelegate:AppDelegate];
	[NSApp finishLaunching];
	[NSApp activateIgnoringOtherApps:YES];
}

/* ----------------------------------------------------------- event loop */

static void pump(NSDate * until)
{
	@autoreleasepool {
		NSEvent * ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:until
		                                     inMode:NSDefaultRunLoopMode dequeue:YES];
		while (ev != nil) {
			[NSApp sendEvent:ev];
			ev = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
			                           inMode:NSDefaultRunLoopMode dequeue:YES];
		}
		[NSApp updateWindows];
	}
}

int RA_Platform_Poll_Event(RA_Event * ev)
{
	if (Queue.empty()) pump([NSDate distantPast]);
	if (Queue.empty()) return 0;
	*ev = Queue.front();
	Queue.pop_front();
	return 1;
}

void RA_Platform_Wait_Event(int milliseconds)
{
	if (!Queue.empty()) return;
	pump([NSDate dateWithTimeIntervalSinceNow:milliseconds / 1000.0]);
}

/* -------------------------------------------------------- key mapping */

/*
**	Mac virtual key code -> Windows virtual-key code, for a US layout -- the
**	layout the game's key bindings and its ToAscii translation assume. Keys
**	with no Windows counterpart map to 0 and are not sent. Command is left to
**	the Mac (Cmd-Q, Cmd-H); Option is Windows' Alt (VK_MENU).
*/
int RA_Input_VK_From_Mac(unsigned short code)
{
	switch (code) {
		case kVK_ANSI_A: return 'A';  case kVK_ANSI_B: return 'B';  case kVK_ANSI_C: return 'C';
		case kVK_ANSI_D: return 'D';  case kVK_ANSI_E: return 'E';  case kVK_ANSI_F: return 'F';
		case kVK_ANSI_G: return 'G';  case kVK_ANSI_H: return 'H';  case kVK_ANSI_I: return 'I';
		case kVK_ANSI_J: return 'J';  case kVK_ANSI_K: return 'K';  case kVK_ANSI_L: return 'L';
		case kVK_ANSI_M: return 'M';  case kVK_ANSI_N: return 'N';  case kVK_ANSI_O: return 'O';
		case kVK_ANSI_P: return 'P';  case kVK_ANSI_Q: return 'Q';  case kVK_ANSI_R: return 'R';
		case kVK_ANSI_S: return 'S';  case kVK_ANSI_T: return 'T';  case kVK_ANSI_U: return 'U';
		case kVK_ANSI_V: return 'V';  case kVK_ANSI_W: return 'W';  case kVK_ANSI_X: return 'X';
		case kVK_ANSI_Y: return 'Y';  case kVK_ANSI_Z: return 'Z';
		case kVK_ANSI_0: return '0';  case kVK_ANSI_1: return '1';  case kVK_ANSI_2: return '2';
		case kVK_ANSI_3: return '3';  case kVK_ANSI_4: return '4';  case kVK_ANSI_5: return '5';
		case kVK_ANSI_6: return '6';  case kVK_ANSI_7: return '7';  case kVK_ANSI_8: return '8';
		case kVK_ANSI_9: return '9';
		case kVK_F1: return 0x70;  case kVK_F2: return 0x71;  case kVK_F3: return 0x72;
		case kVK_F4: return 0x73;  case kVK_F5: return 0x74;  case kVK_F6: return 0x75;
		case kVK_F7: return 0x76;  case kVK_F8: return 0x77;  case kVK_F9: return 0x78;
		case kVK_F10: return 0x79; case kVK_F11: return 0x7A; case kVK_F12: return 0x7B;
		case kVK_Return:         return 0x0D;		/* VK_RETURN */
		case kVK_ANSI_KeypadEnter: return 0x0D;
		case kVK_Tab:            return 0x09;		/* VK_TAB */
		case kVK_Space:          return 0x20;		/* VK_SPACE */
		case kVK_Delete:         return 0x08;		/* VK_BACK: the Mac "delete" is backspace */
		case kVK_ForwardDelete:  return 0x2E;		/* VK_DELETE */
		case kVK_Escape:         return 0x1B;		/* VK_ESCAPE */
		case kVK_Help:           return 0x2D;		/* VK_INSERT: where Insert sits on a Mac keyboard */
		case kVK_Home:           return 0x24;
		case kVK_End:            return 0x23;
		case kVK_PageUp:         return 0x21;
		case kVK_PageDown:       return 0x22;
		case kVK_LeftArrow:      return 0x25;
		case kVK_UpArrow:        return 0x26;
		case kVK_RightArrow:     return 0x27;
		case kVK_DownArrow:      return 0x28;
		case kVK_ANSI_Keypad0: return 0x60; case kVK_ANSI_Keypad1: return 0x61; case kVK_ANSI_Keypad2: return 0x62;
		case kVK_ANSI_Keypad3: return 0x63; case kVK_ANSI_Keypad4: return 0x64; case kVK_ANSI_Keypad5: return 0x65;
		case kVK_ANSI_Keypad6: return 0x66; case kVK_ANSI_Keypad7: return 0x67; case kVK_ANSI_Keypad8: return 0x68;
		case kVK_ANSI_Keypad9: return 0x69;
		case kVK_ANSI_KeypadMultiply: return 0x6A;
		case kVK_ANSI_KeypadPlus:     return 0x6B;
		case kVK_ANSI_KeypadMinus:    return 0x6D;
		case kVK_ANSI_KeypadDecimal:  return 0x6E;
		case kVK_ANSI_KeypadDivide:   return 0x6F;
		case kVK_ANSI_KeypadClear:    return 0x90;	/* VK_NUMLOCK: Clear sits there */
		case kVK_ANSI_Semicolon:    return 0xBA;
		case kVK_ANSI_Equal:        return 0xBB;
		case kVK_ANSI_Comma:        return 0xBC;
		case kVK_ANSI_Minus:        return 0xBD;
		case kVK_ANSI_Period:       return 0xBE;
		case kVK_ANSI_Slash:        return 0xBF;
		case kVK_ANSI_Grave:        return 0xC0;
		case kVK_ANSI_LeftBracket:  return 0xDB;
		case kVK_ANSI_Backslash:    return 0xDC;
		case kVK_ANSI_RightBracket: return 0xDD;
		case kVK_ANSI_Quote:        return 0xDE;
		case kVK_Shift: case kVK_RightShift:     return 0x10;	/* VK_SHIFT */
		case kVK_Control: case kVK_RightControl: return 0x11;	/* VK_CONTROL */
		case kVK_Option: case kVK_RightOption:   return 0x12;	/* VK_MENU (Alt) */
		case kVK_CapsLock:                       return 0x14;	/* VK_CAPITAL */
		default: return 0;
	}
}
