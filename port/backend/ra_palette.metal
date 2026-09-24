#include <metal_stdlib>
using namespace metal;

// Fullscreen triangle: no vertex buffer, positions derived from vertex id.
struct VSOut { float4 pos [[position]]; float2 uv; };

vertex VSOut ra_vs(uint vid [[vertex_id]])
{
    float2 p = float2((vid << 1) & 2, vid & 2);
    VSOut o;
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.uv  = float2(p.x, 1.0 - p.y);
    return o;
}

// The engine hands us an 8-bit palette index per pixel and a separate 256-entry
// palette it mutates directly for fades. Both stay on the GPU: the index buffer
// is r8uint and the palette is a 256x1 RGBA8 texture, so a palette change is a
// 1KB upload, not a re-expansion of the whole framebuffer.
//
// Integer read() rather than a sampler: indices must never be filtered, and
// interpolating between two palette indices is meaningless.
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
