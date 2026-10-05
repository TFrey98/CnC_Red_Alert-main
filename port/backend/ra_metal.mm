/*
**	ra_metal.mm -- native Metal/Cocoa display backend.
**
**	Built WITHOUT -DWIN32 and WITHOUT -include wwcompat.h. See ra_platform.h for
**	why that separation is mandatory rather than tidy.
*/
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "ra_platform.h"
#include "ra_internal.h"

@class RAView;

struct RA_Display {
	int                          width;
	int                          height;
	NSWindow                   * window;
	RAView                     * view;
	CAMetalLayer               * layer;
	id<MTLDevice>                device;
	id<MTLCommandQueue>          queue;
	id<MTLRenderPipelineState>   pipeline;
	id<MTLTexture>               indexTex;    /* w x h, r8uint          */
	id<MTLTexture>               paletteTex;  /* 256 x 1, rgba8unorm    */
};

/*
**	The shader is compiled from source at startup rather than loaded from a
**	prebuilt .metallib. That is a deliberate trade: it costs a few milliseconds
**	once, and in exchange the .app has no build-time Metal toolchain dependency
**	and no resource file that can go missing or out of sync with this code.
**	ra_palette.metal holds the same source for offline compilation and editing.
*/
static NSString * const kShaderSource = @R"METAL(
#include <metal_stdlib>
using namespace metal;

struct VSOut { float4 pos [[position]]; float2 uv; };

vertex VSOut ra_vs(uint vid [[vertex_id]])
{
    float2 p = float2((vid << 1) & 2, vid & 2);
    VSOut o;
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.uv  = float2(p.x, 1.0 - p.y);
    return o;
}

fragment float4 ra_fs(VSOut in [[stage_in]],
                      texture2d<uint,  access::read> indices [[texture(0)]],
                      texture2d<float, access::read> palette [[texture(1)]])
{
    uint2 size = uint2(indices.get_width(), indices.get_height());
    uint2 xy   = uint2(in.uv * float2(size));
    xy = min(xy, size - 1);
    uint idx = indices.read(xy).r;
    return palette.read(uint2(idx, 0));
}
)METAL";

/*
**	The window's content view: draws through its CAMetalLayer, and turns
**	keyboard and mouse NSEvents into RA_Events (ra_input.mm), with positions
**	scaled from window points to framebuffer pixels.
*/
static bool CursorVisible = true;

@interface RAView : NSView <NSWindowDelegate>
@property (nonatomic) RA_Display * display;
@end

@implementation RAView
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)e { return YES; }
- (BOOL)wantsUpdateLayer { return YES; }

- (void)push:(int)type vk:(int)vk repeat:(int)repeat button:(int)button event:(NSEvent *)e
{
	[self push:type vk:vk repeat:repeat button:button at:(e != nil ? [e locationInWindow] : NSMakePoint(NAN, NAN))];
}

/* `w` is in window coordinates; NAN means no position (key events). */
- (void)push:(int)type vk:(int)vk repeat:(int)repeat button:(int)button at:(NSPoint)w
{
	RA_Event r = {type, vk, repeat, button, 0, 0};
	if (!isnan(w.x) && self.display != NULL) {
		NSPoint p = [self convertPoint:w fromView:nil];
		NSRect b = self.bounds;
		int x = (int)(p.x * self.display->width / b.size.width);
		int y = (int)((b.size.height - p.y) * self.display->height / b.size.height);
		r.x = x < 0 ? 0 : (x >= self.display->width ? self.display->width - 1 : x);
		r.y = y < 0 ? 0 : (y >= self.display->height ? self.display->height - 1 : y);
	}
	RA_Input_Push(r);
}

- (void)keyDown:(NSEvent *)e
{
	int vk = RA_Input_VK_From_Mac([e keyCode]);
	if (vk) [self push:RA_EV_KEY_DOWN vk:vk repeat:[e isARepeat] button:0 event:nil];
}
- (void)keyUp:(NSEvent *)e
{
	int vk = RA_Input_VK_From_Mac([e keyCode]);
	if (vk) [self push:RA_EV_KEY_UP vk:vk repeat:0 button:0 event:nil];
}
/* Modifier keys arrive as flag changes, not key events. */
- (void)flagsChanged:(NSEvent *)e
{
	int vk = RA_Input_VK_From_Mac([e keyCode]);
	if (!vk) return;
	NSEventModifierFlags f = [e modifierFlags];
	bool down = false;
	switch (vk) {
		case 0x10: down = (f & NSEventModifierFlagShift) != 0; break;
		case 0x11: down = (f & NSEventModifierFlagControl) != 0; break;
		case 0x12: down = (f & NSEventModifierFlagOption) != 0; break;
		case 0x14: down = (f & NSEventModifierFlagCapsLock) != 0; break;
	}
	[self push:(down ? RA_EV_KEY_DOWN : RA_EV_KEY_UP) vk:vk repeat:0 button:0 event:nil];
}

- (void)mouseMoved:(NSEvent *)e        { [self push:RA_EV_MOUSE_MOVE vk:0 repeat:0 button:0 event:e]; }

/*
**	Mouse moves while the app is in the background. Windows sent WM_MOUSEMOVE
**	to the window under the pointer whether or not it was active; Cocoa only
**	sends mouseMoved: to the key window of the active app. Without this, coming
**	back to the window left the game's cursor where the pointer had exited --
**	usually on an edge, where it also scrolls the map. (NSTrackingActiveAlways;
**	when the app is active the window delivers the moves as well, and a repeated
**	position is harmless.)
*/
- (void)updateTrackingAreas
{
	[super updateTrackingAreas];
	for (NSTrackingArea * a in [self.trackingAreas copy]) [self removeTrackingArea:a];
	[self addTrackingArea:[[NSTrackingArea alloc] initWithRect:NSZeroRect
		options:(NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect)
		owner:self userInfo:nil]];
}

/* Report where the pointer really is, if it is over this view (see RA_Metal_Sync_Pointer). */
- (void)syncPointer
{
	if (self.window == nil) return;
	NSPoint w = [self.window convertPointFromScreen:[NSEvent mouseLocation]];
	NSPoint v = [self convertPoint:w fromView:nil];
	if (NSPointInRect(v, self.bounds)) [self push:RA_EV_MOUSE_MOVE vk:0 repeat:0 button:0 at:w];
}
- (void)mouseDragged:(NSEvent *)e      { [self push:RA_EV_MOUSE_MOVE vk:0 repeat:0 button:0 event:e]; }
- (void)rightMouseDragged:(NSEvent *)e { [self push:RA_EV_MOUSE_MOVE vk:0 repeat:0 button:0 event:e]; }
- (void)otherMouseDragged:(NSEvent *)e { [self push:RA_EV_MOUSE_MOVE vk:0 repeat:0 button:0 event:e]; }
- (void)mouseDown:(NSEvent *)e         { [self push:RA_EV_BUTTON_DOWN vk:0 repeat:0 button:0 event:e]; }
- (void)mouseUp:(NSEvent *)e           { [self push:RA_EV_BUTTON_UP vk:0 repeat:0 button:0 event:e]; }
- (void)rightMouseDown:(NSEvent *)e    { [self push:RA_EV_BUTTON_DOWN vk:0 repeat:0 button:1 event:e]; }
- (void)rightMouseUp:(NSEvent *)e      { [self push:RA_EV_BUTTON_UP vk:0 repeat:0 button:1 event:e]; }
- (void)otherMouseDown:(NSEvent *)e    { [self push:RA_EV_BUTTON_DOWN vk:0 repeat:0 button:2 event:e]; }
- (void)otherMouseUp:(NSEvent *)e      { [self push:RA_EV_BUTTON_UP vk:0 repeat:0 button:2 event:e]; }

/*
**	When the game draws its own cursor, the Mac pointer is made invisible over
**	the window only -- outside it, the desktop pointer behaves normally.
*/
- (void)resetCursorRects
{
	if (CursorVisible) return;
	static NSCursor * blank = nil;
	if (blank == nil) {
		NSImage * img = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
		blank = [[NSCursor alloc] initWithImage:img hotSpot:NSZeroPoint];
	}
	[self addCursorRect:self.bounds cursor:blank];
}

- (void)setFrameSize:(NSSize)size
{
	[super setFrameSize:size];
	if (self.display != NULL && self.display->layer != nil) {
		CGFloat scale = self.window ? self.window.backingScaleFactor : 1.0;
		self.display->layer.contentsScale = scale;
		self.display->layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
	}
}

/* The close button asks the game to quit; the game decides how. */
- (BOOL)windowShouldClose:(NSWindow *)w
{
	[self push:RA_EV_QUIT vk:0 repeat:0 button:0 event:nil];
	return NO;
}
@end

/*
**	Called when the app becomes active: the game's idea of the pointer is
**	whatever was last reported, which may be from before focus was lost.
*/
void RA_Metal_Sync_Pointer(void)
{
	for (NSWindow * w in [NSApp windows]) {
		if ([w.contentView isKindOfClass:[RAView class]]) [(RAView *)w.contentView syncPointer];
	}
}

void RA_Platform_Set_Cursor_Visible(int visible)
{
	CursorVisible = visible != 0;
	for (NSWindow * w in [NSApp windows]) [w invalidateCursorRectsForView:w.contentView];
}

static void make_index_texture(RA_Display * d)
{
	MTLTextureDescriptor * itd =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
		                                                   width:(NSUInteger)d->width
		                                                  height:(NSUInteger)d->height
		                                               mipmapped:NO];
	itd.usage = MTLTextureUsageShaderRead;
	d->indexTex = [d->device newTextureWithDescriptor:itd];
}

/*
**	Window size for a framebuffer: twice its pixels, the 640 x 400 game shown
**	at 4:3 as on a CRT (400 lines stretched to 480), never larger than the screen.
*/
static NSSize window_size_for(int width, int height)
{
	double h = (width * 3.0 / 4.0 > height) ? width * 3.0 / 4.0 : height;
	NSSize s = NSMakeSize(width * 2.0, h * 2.0);
	NSRect vis = [[NSScreen mainScreen] visibleFrame];
	double fit = 1.0;
	if (s.width > vis.size.width * 0.9) fit = vis.size.width * 0.9 / s.width;
	if (s.height * fit > vis.size.height * 0.9) fit = vis.size.height * 0.9 / s.height;
	return NSMakeSize(floor(s.width * fit), floor(s.height * fit));
}

RA_Display * RA_Display_Create(int width, int height, const char * title)
{
	if (width <= 0 || height <= 0) return NULL;

	RA_Display * d = (RA_Display *)calloc(1, sizeof(RA_Display));
	if (d == NULL) return NULL;
	d->width  = width;
	d->height = height;

	d->device = MTLCreateSystemDefaultDevice();
	if (d->device == nil) { free(d); return NULL; }
	d->queue = [d->device newCommandQueue];

	NSError * err = nil;
	id<MTLLibrary> lib = [d->device newLibraryWithSource:kShaderSource
	                                             options:nil
	                                               error:&err];
	if (lib == nil) {
		NSLog(@"RA: shader compile failed: %@", err);
		free(d);
		return NULL;
	}

	MTLRenderPipelineDescriptor * pd = [[MTLRenderPipelineDescriptor alloc] init];
	pd.vertexFunction   = [lib newFunctionWithName:@"ra_vs"];
	pd.fragmentFunction = [lib newFunctionWithName:@"ra_fs"];
	pd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
	d->pipeline = [d->device newRenderPipelineStateWithDescriptor:pd error:&err];
	if (d->pipeline == nil) {
		NSLog(@"RA: pipeline creation failed: %@", err);
		free(d);
		return NULL;
	}

	/*
	**	r8uint, not r8unorm: palette indices are identifiers, not intensities.
	**	Any filtering or normalisation of them is meaningless -- interpolating
	**	between index 3 and index 4 does not give a colour between two palette
	**	entries, it gives an unrelated one.
	*/
	make_index_texture(d);

	MTLTextureDescriptor * ptd =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		                                                   width:256
		                                                  height:1
		                                               mipmapped:NO];
	ptd.usage = MTLTextureUsageShaderRead;
	d->paletteTex = [d->device newTextureWithDescriptor:ptd];

	RA_Platform_Init();
	NSSize size = window_size_for(width, height);
	d->window = [[NSWindow alloc]
		initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
		          styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		                     NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
		            backing:NSBackingStoreBuffered
		              defer:NO];
	[d->window setTitle:[NSString stringWithUTF8String:(title ? title : "Red Alert")]];
	[d->window setContentAspectRatio:size];
	[d->window setAcceptsMouseMovedEvents:YES];
	[d->window setReleasedWhenClosed:NO];

	d->layer = [CAMetalLayer layer];
	d->layer.device      = d->device;
	d->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	d->layer.framebufferOnly = YES;

	d->view = [[RAView alloc] initWithFrame:NSMakeRect(0, 0, size.width, size.height)];
	d->view.display = d;
	[d->view setWantsLayer:YES];
	[d->view setLayer:d->layer];
	[d->window setContentView:d->view];
	[d->window setDelegate:d->view];
	[d->window makeFirstResponder:d->view];
	[d->view setFrameSize:size];
	[d->window center];

	return d;
}

void RA_Display_Destroy(RA_Display * d)
{
	if (d == NULL) return;
	[d->window orderOut:nil];
	d->view.display = NULL;
	free(d);
}

void RA_Display_Show(RA_Display * d)
{
	if (d == NULL) return;
	[d->window makeKeyAndOrderFront:nil];
	[NSApp activateIgnoringOtherApps:YES];
}

void RA_Display_Resize(RA_Display * d, int width, int height)
{
	if (d == NULL || width <= 0 || height <= 0) return;
	if (width == d->width && height == d->height) return;
	d->width = width;
	d->height = height;
	make_index_texture(d);
	NSSize size = window_size_for(width, height);
	[d->window setContentAspectRatio:size];
	[d->window setContentSize:size];
	[d->view setFrameSize:size];
}

void RA_Display_SetPalette(RA_Display * d, const unsigned char * rgba)
{
	if (d == NULL || rgba == NULL) return;
	[d->paletteTex replaceRegion:MTLRegionMake2D(0, 0, 256, 1)
	                 mipmapLevel:0
	                   withBytes:rgba
	                 bytesPerRow:256 * 4];
}

void RA_Display_Present(RA_Display * d, const unsigned char * indices)
{
	if (d == NULL || indices == NULL) return;

	[d->indexTex replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)d->width, (NSUInteger)d->height)
	               mipmapLevel:0
	                 withBytes:indices
	               bytesPerRow:(NSUInteger)d->width];

	id<CAMetalDrawable> drawable = [d->layer nextDrawable];
	if (drawable == nil) return;

	MTLRenderPassDescriptor * rp = [MTLRenderPassDescriptor renderPassDescriptor];
	rp.colorAttachments[0].texture     = drawable.texture;
	rp.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
	rp.colorAttachments[0].storeAction = MTLStoreActionStore;

	id<MTLCommandBuffer> cb = [d->queue commandBuffer];
	id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
	[enc setRenderPipelineState:d->pipeline];
	[enc setFragmentTexture:d->indexTex   atIndex:0];
	[enc setFragmentTexture:d->paletteTex atIndex:1];
	/* Fullscreen triangle: three vertices, no vertex buffer. */
	[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[enc endEncoding];
	[cb presentDrawable:drawable];
	[cb commit];
}
