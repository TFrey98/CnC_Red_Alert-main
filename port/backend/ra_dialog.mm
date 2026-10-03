/*
**	ra_dialog.mm -- native dialogs the engine asks for through ra_platform.h.
**
**	Built with $RA_OBJCXXFLAGS (no Win32 shim); see ra_platform.h for why.
*/
#import <Cocoa/Cocoa.h>
#include "ra_platform.h"

/* Win32-style codes from the compat layer's GetLastError(); see compat/windows.h. */
static NSString * ra_describe_error(int code)
{
	switch (code) {
		case 2:   return @"The file could not be found.";
		case 3:   return @"The folder containing the file could not be found.";
		case 5:   return @"Access to the file was denied.";
		case 6:   return @"The file is no longer open.";
		case 112: return @"The disk is full.";
		default:  return [NSString stringWithFormat:@"The system reported error %d.", code];
	}
}

int RA_Platform_Disk_Error(const char * filename, int error_code)
{
	__block int choice = RA_DISK_ERROR_CANCEL;
	void (^ask)(void) = ^{
		[NSApplication sharedApplication];
		NSAlert * alert = [[NSAlert alloc] init];
		alert.alertStyle = NSAlertStyleWarning;
		alert.messageText = @"Red Alert could not read a file.";
		NSString * name = filename ? [NSString stringWithUTF8String:filename] : @"(unknown file)";
		alert.informativeText = [NSString stringWithFormat:@"%@\n\n%@\n\nChoose Try Again to retry the read, or Cancel to quit the game.",
		                         name, ra_describe_error(error_code)];
		[alert addButtonWithTitle:@"Try Again"];	/* first button: default, Return */
		[alert addButtonWithTitle:@"Cancel"];		/* second button: Escape */
		[NSApp activateIgnoringOtherApps:YES];
		choice = ([alert runModal] == NSAlertFirstButtonReturn) ? RA_DISK_ERROR_RETRY : RA_DISK_ERROR_CANCEL;
	};
	/* AppKit dialogs must run on the main thread. */
	if ([NSThread isMainThread]) ask(); else dispatch_sync(dispatch_get_main_queue(), ask);
	return choice;
}
