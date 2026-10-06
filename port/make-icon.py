#!/usr/bin/env python3
"""make-icon.py -- Westwood's REDALERT.ICO as a macOS .icns.

    python3 port/make-icon.py CODE/REDALERT.ICO out.icns

The original icon (CODE/REDALERT.ICO, linked into RA95.EXE by CC_ICON.RC) is
32x32 at most. It is scaled up by whole pixels, nearest neighbour, so it keeps
its look instead of blurring. Transparency comes from the icon's AND mask.

Standard library only: the ICO's bitmap is decoded here and the PNGs written
with zlib; `iconutil` (part of macOS) builds the .icns.
"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib


def largest_image(ico):
    """The (width, height, bytes) of the ICO's largest, deepest image."""
    count = struct.unpack_from('<H', ico, 4)[0]
    best = None
    for i in range(count):
        w, h, colors, _, _, bpp, size, offset = struct.unpack_from('<BBBBHHII', ico, 6 + 16 * i)
        w, h = w or 256, h or 256
        data = ico[offset:offset + size]
        depth = struct.unpack_from('<H', data, 14)[0]	# from the bitmap header; the directory's is often 0
        key = (w * h, depth)
        if best is None or key > best[0]:
            best = (key, w, h, data)
    return best[1], best[2], best[3]


def decode(w, h, dib):
    """RGBA rows, top first, from an ICO's DIB (1/4/8 bpp paletted, 24 or 32 bpp)."""
    hsize, _, _, _, bpp = struct.unpack_from('<IiiHH', dib, 0)
    colors_used = struct.unpack_from('<I', dib, 32)[0]
    pos = hsize
    palette = []
    if bpp <= 8:
        n = colors_used or (1 << bpp)
        for i in range(n):
            b, g, r, _ = dib[pos + 4 * i:pos + 4 * i + 4]
            palette.append((r, g, b))
        pos += 4 * n
    xor_stride = ((w * bpp + 31) // 32) * 4
    and_stride = ((w + 31) // 32) * 4
    xor = dib[pos:pos + xor_stride * h]
    mask = dib[pos + xor_stride * h:pos + xor_stride * h + and_stride * h]
    rows = []
    for y in range(h):
        src = h - 1 - y						# bitmaps are stored bottom row first
        row = []
        for x in range(w):
            if bpp <= 8:
                bit = x * bpp
                byte = xor[src * xor_stride + bit // 8]
                index = (byte >> (8 - bpp - bit % 8)) & ((1 << bpp) - 1)
                r, g, b = palette[index]
                a = 255
            else:
                o = src * xor_stride + x * (bpp // 8)
                b, g, r = xor[o:o + 3]
                a = xor[o + 3] if bpp == 32 else 255
            if mask and (mask[src * and_stride + x // 8] >> (7 - x % 8)) & 1:
                a = 0
            row.append((r, g, b, a))
        rows.append(row)
    return rows


def write_png(path, rows):
    h, w = len(rows), len(rows[0])
    raw = b''.join(b'\0' + bytes(c for px in row for c in px) for row in rows)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


def scaled(rows, size):
    h, w = len(rows), len(rows[0])
    return [[rows[y * h // size][x * w // size] for x in range(size)] for y in range(size)]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    w, h, dib = largest_image(open(sys.argv[1], 'rb').read())
    rows = decode(w, h, dib)
    with tempfile.TemporaryDirectory() as tmp:
        iconset = os.path.join(tmp, 'icon.iconset')
        os.mkdir(iconset)
        for size in (16, 32, 128, 256, 512):
            write_png(os.path.join(iconset, f'icon_{size}x{size}.png'), scaled(rows, size))
            write_png(os.path.join(iconset, f'icon_{size}x{size}@2x.png'), scaled(rows, size * 2))
        subprocess.run(['iconutil', '-c', 'icns', iconset, '-o', sys.argv[2]], check=True)


if __name__ == '__main__':
    main()
