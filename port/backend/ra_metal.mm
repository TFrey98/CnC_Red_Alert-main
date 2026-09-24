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

struct RA_Display {
	int                          width;
	int                          height;
	NSWindow                   * window;
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
	MTLTextureDescriptor * itd =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Uint
		                                                   width:(NSUInteger)width
		                                                  height:(NSUInteger)height
		                                               mipmapped:NO];
	itd.usage = MTLTextureUsageShaderRead;
	d->indexTex = [d->device newTextureWithDescriptor:itd];

	MTLTextureDescriptor * ptd =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		                                                   width:256
		                                                  height:1
		                                               mipmapped:NO];
	ptd.usage = MTLTextureUsageShaderRead;
	d->paletteTex = [d->device newTextureWithDescriptor:ptd];

	NSRect frame = NSMakeRect(0, 0, width * 2, height * 2);
	d->window = [[NSWindow alloc]
		initWithContentRect:frame
		          styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		                     NSWindowStyleMaskMiniaturizable)
		            backing:NSBackingStoreBuffered
		              defer:NO];
	[d->window setTitle:[NSString stringWithUTF8String:(title ? title : "Red Alert")]];

	d->layer = [CAMetalLayer layer];
	d->layer.device      = d->device;
	d->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	d->layer.framebufferOnly = YES;
	[d->window.contentView setLayer:d->layer];
	[d->window.contentView setWantsLayer:YES];

	return d;
}

void RA_Display_Destroy(RA_Display * d)
{
	if (d == NULL) return;
	free(d);
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
