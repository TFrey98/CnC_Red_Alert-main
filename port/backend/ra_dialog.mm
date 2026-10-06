/*
**	ra_dialog.mm -- native dialogs the engine asks for through ra_platform.h.
**
**	Built with $RA_OBJCXXFLAGS (no Win32 shim); see ra_platform.h for why.
*/
#import <Cocoa/Cocoa.h>
#include "ra_platform.h"
#include <dlfcn.h>

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

/*
**	The game's MessageBox. Its text was written for a Windows dialog; it is
**	shown as written (the caption becomes the alert's title line).
*/
int RA_Platform_Message_Box(const char * text, const char * caption, int buttons, int warning)
{
	__block int choice = RA_ID_OK;
	void (^ask)(void) = ^{
		[NSApplication sharedApplication];
		NSAlert * alert = [[NSAlert alloc] init];
		alert.alertStyle = warning ? NSAlertStyleWarning : NSAlertStyleInformational;
		alert.messageText = caption ? [NSString stringWithUTF8String:caption] : @"Red Alert";
		NSString * body = text ? [NSString stringWithCString:text encoding:NSWindowsCP1252StringEncoding] : @"";
		alert.informativeText = body ? body : @"";
		switch (buttons) {
			case RA_MB_YESNO:    [alert addButtonWithTitle:@"Yes"]; [alert addButtonWithTitle:@"No"]; break;
			case RA_MB_OKCANCEL: [alert addButtonWithTitle:@"OK"]; [alert addButtonWithTitle:@"Cancel"]; break;
			default:             [alert addButtonWithTitle:@"OK"]; break;
		}
		[NSApp activateIgnoringOtherApps:YES];
		bool first = [alert runModal] == NSAlertFirstButtonReturn;
		switch (buttons) {
			case RA_MB_YESNO:    choice = first ? RA_ID_YES : RA_ID_NO; break;
			case RA_MB_OKCANCEL: choice = first ? RA_ID_OK : RA_ID_CANCEL; break;
			default:             choice = RA_ID_OK; break;
		}
	};
	if ([NSThread isMainThread]) ask(); else dispatch_sync(dispatch_get_main_queue(), ask);
	return choice;
}

/*
**	The data folder. Saved in the app's preferences (NSUserDefaults), so the
**	.app and the bare executable each remember their own.
*/
static NSString * const kDataFolderKey = @"DataFolder";

static int ra_copy_path(NSString * path, char * out, int size)
{
	if (path == nil || out == NULL || size <= 0) return 0;
	const char * utf8 = [path fileSystemRepresentation];
	if (utf8 == NULL || (int)strlen(utf8) >= size) return 0;
	strcpy(out, utf8);
	return 1;
}

int RA_Platform_Saved_Data_Folder(char * out, int size)
{
	@autoreleasepool {
		return ra_copy_path([[NSUserDefaults standardUserDefaults] stringForKey:kDataFolderKey], out, size);
	}
}

void RA_Platform_Save_Data_Folder(const char * path)
{
	@autoreleasepool {
		if (path == NULL) return;
		NSString * p = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:path length:strlen(path)];
		[[NSUserDefaults standardUserDefaults] setObject:p forKey:kDataFolderKey];
	}
}

int RA_Platform_Choose_Data_Folder(char * out, int size, const char * message)
{
	__block int chosen = 0;
	void (^ask)(void) = ^{
		[NSApplication sharedApplication];
		NSOpenPanel * panel = [NSOpenPanel openPanel];
		panel.canChooseDirectories = YES;
		panel.canChooseFiles = NO;
		panel.allowsMultipleSelection = NO;
		panel.prompt = @"Choose";
		panel.message = message ? [NSString stringWithUTF8String:message] : @"";
		[NSApp activateIgnoringOtherApps:YES];
		if ([panel runModal] == NSModalResponseOK) {
			chosen = ra_copy_path(panel.URL.path, out, size);
		}
	};
	if ([NSThread isMainThread]) ask(); else dispatch_sync(dispatch_get_main_queue(), ask);
	return chosen;
}

int RA_Platform_Option_Key_Down(void)
{
	return ([NSEvent modifierFlags] & NSEventModifierFlagOption) ? 1 : 0;
}

/*
**	App translocation. macOS runs a quarantined app (downloaded, then opened
**	where it was unzipped) from a read-only copy at a random path, so the
**	bundle's own location says nothing about the folder the player sees. The
**	Security framework can map it back; the calls are exported but have no
**	public header, so they are looked up at run time and skipped if absent.
*/
typedef Boolean (*SecTranslocateIsTranslocatedURLFn)(CFURLRef, bool *, CFErrorRef *);
typedef CFURLRef (*SecTranslocateCreateOriginalPathForURLFn)(CFURLRef, CFErrorRef *);

static NSURL * ra_untranslocated(NSURL * url)
{
	void * security = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_LAZY);
	if (security == NULL) return url;
	SecTranslocateIsTranslocatedURLFn is_translocated =
		(SecTranslocateIsTranslocatedURLFn)dlsym(security, "SecTranslocateIsTranslocatedURL");
	SecTranslocateCreateOriginalPathForURLFn original_path =
		(SecTranslocateCreateOriginalPathForURLFn)dlsym(security, "SecTranslocateCreateOriginalPathForURL");
	bool translocated = false;
	if (is_translocated == NULL || original_path == NULL
	    || !is_translocated((__bridge CFURLRef)url, &translocated, NULL) || !translocated) {
		return url;
	}
	CFURLRef original = original_path((__bridge CFURLRef)url, NULL);
	return original ? (NSURL *)CFBridgingRelease(original) : url;
}

int RA_Platform_App_Folder(char * out, int size)
{
	@autoreleasepool {
		NSURL * bundle = [[NSBundle mainBundle] bundleURL];
		if (bundle == nil || ![[bundle pathExtension] isEqualToString:@"app"]) return 0;
		return ra_copy_path([[ra_untranslocated(bundle) URLByDeletingLastPathComponent] path], out, size);
	}
}
