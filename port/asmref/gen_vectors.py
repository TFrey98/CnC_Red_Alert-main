#!/usr/bin/env python3
"""
gen_vectors.py [routine ...] -- run Westwood's ORIGINAL assembly under an x86
emulator on generated cases and write port/tests/asm_vectors/<routine>.txt.

The C++ tests in port/tests (asm_*.cpp) replay those files against the C
translations, so the regression suite needs neither Python nor Unicorn; only
regenerating the vectors does (port/asmref/setup.sh).

Line format, all integers in decimal:   <inputs ...> : <outputs ...>
Large inputs are not stored: a buffer is described by a size and a seed, and
both sides fill it with the same xorshift32 stream (fill() below and in
port/tests/asm_replay.h). Outputs are return values and FNV-1a-64 hashes of
every byte of every buffer the routine could touch, INCLUDING guard bytes
around them, so an out-of-bounds write shows up as a mismatch.
"""
import os, random, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from x86ref import Machine

OUT = os.path.join(ROOT, 'port/tests/asm_vectors')
GUARD = 64
M32 = 0xFFFFFFFF


def fill(seed, n):
    s = seed & M32 or 0x9E3779B9
    out = bytearray(n)
    for i in range(n):
        s ^= (s << 13) & M32
        s ^= s >> 17
        s ^= (s << 5) & M32
        out[i] = s >> 24
    return bytes(out)


def fnv(data):
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


class View:
    """A view port in emulated memory: guard + Height lines of Stride bytes + guard."""
    def __init__(self, m, w, h, xadd, pitch, align, seed):
        self.m, self.w, self.h, self.xadd, self.pitch, self.align, self.seed = m, w, h, xadd, pitch, align, seed
        self.size = GUARD + align + (w + xadd + pitch) * h + GUARD
        self.base = m.put(fill(seed, self.size))
        self.offset = self.base + GUARD + align
        self.gvp = m.put(struct.pack('<8I', self.offset, w, h, xadd, 0, 0, pitch, 0))

    def params(self):
        return [self.w, self.h, self.xadd, self.pitch, self.align, self.seed]

    def hash(self):
        return fnv(self.m.read(self.base, self.size))


def s32(v):
    return v - (1 << 32) if v & 0x80000000 else v


def rand_view(m, r, wmax=48, hmax=24):
    return View(m, r.randint(1, wmax), r.randint(1, hmax), r.choice([0, 0, r.randint(1, 9)]),
                r.choice([0, 0, r.randint(1, 7)]), r.randint(0, 3), r.getrandbits(32))


def coord(r, lim):
    return r.choice([r.randint(-8, lim + 8), r.randint(-40000, 40000), r.randint(0, lim - 1),
                     -1, 0, lim - 1, lim, r.randint(-70000, 70000)])


# --------------------------------------------------------------------------
# Routines. Each yields lines; `m` is a fresh-heap Machine per case.
# --------------------------------------------------------------------------
def gen_buffer_clear(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/CLEAR.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r, wmax=r.choice([4, 13, 14, 15, 64]))
        color = r.getrandbits(8)
        m.call('buffer_clear', v.gvp, color)
        yield v.params() + [color], [v.hash()]


def gen_buffer_put_pixel(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/PUTPIX.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r)
        x, y, color = coord(r, v.w), coord(r, v.h), r.getrandbits(8)
        m.call('buffer_put_pixel', v.gvp, x, y, color)
        yield v.params() + [x, y, color], [v.hash()]


def gen_buffer_get_pixel(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/GETPIX.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r)
        x, y = coord(r, v.w), coord(r, v.h)
        ret = m.call('buffer_get_pixel', v.gvp, x, y)
        yield v.params() + [x, y], [s32(ret), v.hash()]


def gen_buffer_fill_rect(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/FILLRECT.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r, wmax=r.choice([8, 20, 64]))
        xs = [coord(r, v.w) for _ in range(2)]
        ys = [coord(r, v.h) for _ in range(2)]
        color = r.getrandbits(8)
        m.call('buffer_fill_rect', v.gvp, xs[0], ys[0], xs[1], ys[1], color)
        yield v.params() + [xs[0], ys[0], xs[1], ys[1], color], [v.hash()]


def region(r, v, wild=True):
    """A region start and extent around the view: mostly overlapping, some wild.
    Wild regions only where nothing outside the view is touched: with a linear
    buffer behind the region, the original computes a wrapped pointer and
    writes through it, which is not a behaviour to compare."""
    if not wild or r.random() < 0.85:
        w = r.randint(-3, v.w + 10); h = r.randint(-3, v.h + 10)
        x = r.randint(-abs(w) - 5, v.w + 5); y = r.randint(-abs(h) - 5, v.h + 5)
    else:
        x, y, w, h = (r.randint(-2**31, 2**31 - 1) for _ in range(4))
    return x, y, w, h


def linear(m, r, w, h):
    """A linear buffer for w x h pixels plus guards; returns (base, size, addr, seed)."""
    n = max(0, w) * max(0, h) if abs(w) < 4096 and abs(h) < 4096 else 0
    seed = r.getrandbits(32)
    size = GUARD + n + GUARD
    base = m.put(fill(seed, size))
    return base, size, base + GUARD, seed


def gen_buffer_remap(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/REMAP.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r)
        x, y, w, h = region(r, v)
        tseed = r.getrandbits(32)
        table = 0 if r.random() < 0.03 else m.put(fill(tseed, 256))
        m.call('buffer_remap', v.gvp, x, y, w, h, table)
        yield v.params() + [x, y, w, h, tseed if table else 0], [v.hash()]


def gen_buffer_to_buffer(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/TOBUFF.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r)
        x, y, w, h = region(r, v, wild=False)
        base, size, addr, seed = linear(m, r, w, h)
        bsize = r.choice([size - 2 * GUARD, max(0, w) * max(0, h) // 2, 0x7FFFFFFF, r.randint(-5, 2000)])
        ret = m.call('buffer_to_buffer', v.gvp, x, y, w, h, addr, bsize)
        yield v.params() + [x, y, w, h, seed, bsize], [s32(ret), v.hash(), fnv(m.read(base, size))]


def gen_buffer_to_page(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/TOPAGE.ASM')
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r)
        x, y, w, h = region(r, v, wild=False)
        base, size, addr, seed = linear(m, r, w, h)
        src = 0 if r.random() < 0.03 else addr
        m.call('buffer_to_page', x, y, w, h, src, v.gvp)
        yield v.params() + [x, y, w, h, seed, 1 if src else 0], [v.hash(), fnv(m.read(base, size))]


def gen_buffer_draw_line(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/DRAWLINE.ASM')
    made = skipped = 0
    while made < n:
        m.reset_heap()
        v = rand_view(m, r)
        k = r.random()
        if k < 0.6:   pts = [r.randint(-30, v.w + 30), r.randint(-30, v.h + 30), r.randint(-30, v.w + 30), r.randint(-30, v.h + 30)]
        elif k < 0.9: pts = [r.randint(-5000, 5000) for _ in range(4)]
        else:         pts = [r.randint(-2**20, 2**20) for _ in range(4)]
        if r.random() < 0.1: pts[3] = pts[1]                    # horizontal
        if r.random() < 0.1: pts[2] = pts[0]                    # vertical
        color = r.getrandbits(32)
        try:
            m.call('buffer_draw_line', v.gvp, *pts, color, limit=5_000_000)
        except RuntimeError:
            skipped += 1                                        # the original faults (#DE) or hangs: nothing to compare
            continue
        made += 1
        yield v.params() + pts + [color], [v.hash()]
    if skipped:
        print(f'  buffer_draw_line: {skipped} generated cases skipped -- the original faults on them', file=sys.stderr)


class Block:
    """A raw pixel buffer, stride x rows between guards; views are windows on it."""
    def __init__(self, m, stride, rows, seed):
        self.m, self.stride, self.rows, self.seed = m, stride, rows, seed
        self.size = GUARD + stride * rows + GUARD
        self.base = m.put(fill(seed, self.size))

    def view(self, ox, oy, w, h):
        off = self.base + GUARD + oy * self.stride + ox
        return self.m.put(struct.pack('<8I', off, w, h, self.stride - w, 0, 0, 0, 0))

    def hash(self):
        return fnv(self.m.read(self.base, self.size))


def rand_window(r, b):
    ox, oy = r.randint(0, b.stride - 1), r.randint(0, b.rows - 1)
    return ox, oy, r.randint(1, b.stride - ox), r.randint(1, b.rows - oy)


def gen_linear_blit_to_linear(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/BITBLIT.ASM')
    for _ in range(n):
        m.reset_heap()
        a = Block(m, r.randint(4, 56), r.randint(2, 24), r.getrandbits(32))
        same = r.random() < 0.5
        b = a if same else Block(m, r.randint(4, 56), r.randint(2, 24), r.getrandbits(32))
        sw, dw = rand_window(r, a), rand_window(r, b)
        sv, dv = a.view(*sw), b.view(*dw)
        pw, ph = r.randint(-2, sw[2] + 6), r.randint(-2, sw[3] + 6)
        x, y = r.randint(-pw - 3, sw[2] + 3), r.randint(-ph - 3, sw[3] + 3)
        dx, dy = r.randint(-pw - 3, dw[2] + 3), r.randint(-ph - 3, dw[3] + 3)
        if same and r.random() < 0.4:                    # strongly overlapping copies
            dx, dy = x + r.randint(-3, 3), y + r.randint(-2, 2)
        trans = r.choice([0, 1, 0, 2, 3])
        ret = m.call('linear_blit_to_linear', sv, dv, x, y, dx, dy, pw, ph, trans)
        yield ([a.stride, a.rows, a.seed, int(same), b.stride, b.rows, b.seed] + list(sw) + list(dw)
               + [x, y, dx, dy, pw, ph, trans]), [s32(ret), a.hash(), 0 if same else b.hash()]


def gen_linear_scale_to_linear(r, n):
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/SCALE.ASM')
    made = skipped = 0
    while made < n:
        m.reset_heap()
        a = Block(m, r.randint(4, 64), r.randint(2, 40), r.getrandbits(32))
        same = r.random() < 0.3
        b = a if same else Block(m, r.randint(4, 64), r.randint(2, 40), r.getrandbits(32))
        sw, dw = rand_window(r, a), rand_window(r, b)
        sv, dv = a.view(*sw), b.view(*dw)
        srcw, srch = r.randint(1, sw[2] + 4), r.randint(1, sw[3] + 4)
        dstw, dsth = r.randint(1, 3 * dw[2] + 4), r.randint(1, 3 * dw[3] + 4)
        if r.random() < 0.03: srcw = 0
        sx, sy = r.randint(-srcw // 2 - 2, sw[2] - srcw // 2), r.randint(-srch // 2 - 2, sw[3] - srch // 2)
        dx, dy = r.randint(-dstw // 2 - 2, dw[2] + 2), r.randint(-dsth // 2 - 2, dw[3] + 2)
        trans = r.choice([0, 0, 1, 5])
        tseed = r.getrandbits(32) if r.random() < 0.4 else 0
        table = m.put(fill(tseed, 256)) if tseed else 0
        try:
            m.call('linear_scale_to_linear', sv, dv, sx, sy, dx, dy, srcw, srch, dstw, dsth, trans, table)
        except RuntimeError:
            skipped += 1
            continue
        made += 1
        yield ([a.stride, a.rows, a.seed, int(same), b.stride, b.rows, b.seed] + list(sw) + list(dw)
               + [sx, sy, dx, dy, srcw, srch, dstw, dsth, trans, tseed]), [a.hash(), 0 if same else b.hash()]
    if skipped:
        print(f'  linear_scale_to_linear: {skipped} generated cases skipped -- the original faults on them', file=sys.stderr)


def iconset(w, h, count, seed):
    """An icon set: 40-byte IControl_Type, icons, a 256-byte map, transparency flags.
    Mirrored by make_iconset() in port/tests/asm_replay.h."""
    nicons = count * w * h
    body = bytearray(fill(seed, nicons + 256 + count))
    for i in range(256):
        body[nicons + i] %= (count + 2)                 # mostly valid icon numbers
    for i in range(count):
        body[nicons + 256 + i] &= 1
    icons, mapo, trans = 40, 40 + nicons, 40 + nicons + 256
    hdr = struct.pack('<6h7i', w, h, count, 0, 0, 0, 40 + len(body), icons, 0, 0, trans, 0, mapo)
    assert len(hdr) == 40
    return hdr + bytes(body)


def icon_number(r, w, h, count):
    """An icon number whose map lookup (map[icon]) stays inside the icon set:
    beyond it both versions would read whatever memory follows, which cannot
    be the same in the emulator and in the test. Real callers pass 0..255."""
    lo = -min(count * w * h, 8)
    return r.choice([r.randint(0, 255), r.randint(0, 255), r.randint(256, 255 + count), r.randint(lo, -1) if lo < 0 else 0])


def stamp_machine():
    return Machine.from_asm('WIN32LIB/DRAWBUFF/STAMP.ASM')


def gen_buffer_draw_stamp_clip(r, n):
    m = stamp_machine()
    for _ in range(n):
        m.reset_heap()
        m.write(m.sym['lasticonset'], b'\0' * 4)
        v = rand_view(m, r, wmax=64, hmax=48)
        iw, ih, cnt, iseed = r.choice([24, 24, r.randint(1, 30)]), r.choice([24, 24, r.randint(1, 30)]), r.randint(1, 40), r.getrandbits(32)
        ic = m.put(iconset(iw, ih, cnt, iseed))
        icon = icon_number(r, iw, ih, cnt)
        minx, miny = r.randint(0, v.w - 1), r.randint(0, v.h - 1)
        maxx, maxy = r.randint(1, v.w - minx), r.randint(1, v.h - miny)
        x, y = r.randint(-iw - 3, maxx + 3), r.randint(-ih - 3, maxy + 3)
        tseed = r.getrandbits(32) if r.random() < 0.3 else 0
        table = m.put(fill(tseed, 256)) if tseed else 0
        m.call('buffer_draw_stamp_clip', v.gvp, ic, icon, x, y, table, minx, miny, maxx, maxy)
        yield v.params() + [iw, ih, cnt, iseed, icon, x, y, tseed, minx, miny, maxx, maxy], [v.hash()]


def gen_buffer_draw_stamp(r, n):
    m = stamp_machine()
    for _ in range(n):
        m.reset_heap()
        m.write(m.sym['lasticonset'], b'\0' * 4)
        iw, ih, cnt, iseed = r.choice([24, 24, r.randint(1, 30)]), r.choice([24, 24, r.randint(4, 30)]), r.randint(1, 40), r.getrandbits(32)
        v = View(m, iw + r.randint(0, 40), ih + r.randint(0, 30), r.choice([0, r.randint(1, 9)]), r.choice([0, r.randint(1, 7)]),
                 r.randint(0, 3), r.getrandbits(32))
        ic = m.put(iconset(iw, ih, cnt, iseed))
        icon = icon_number(r, iw, ih, cnt)
        x, y = r.randint(0, v.w - iw), r.randint(0, v.h - ih)
        tseed = r.getrandbits(32) if r.random() < 0.3 else 0
        table = m.put(fill(tseed, 256)) if tseed else 0
        m.call('buffer_draw_stamp', v.gvp, ic, icon, x, y, table)
        yield v.params() + [iw, ih, cnt, iseed, icon, x, y, tseed], [v.hash()]


def gen_is_icon_cached(r, n):
    """Is_Icon_Cached with the C++ side (ICONCACH.CPP) replaced by stubs that
    behave as port/tests/asm_drawbuff.cpp's: slots come from a script, and
    Cache_New_Icon succeeds per the script and logs what it was given."""
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX
    state = {}
    def free_slot(mm, _):
        s = state['slots'][state['n'] % len(state['slots'])]; state['n'] += 1
        return s
    def new_icon(mm, _):
        slot = mm.uc.reg_read(UC_X86_REG_EAX); ptr = mm.uc.reg_read(UC_X86_REG_EDX)
        state['log'].append((slot, ptr - state['ic']))
        ok = state['ok'][len(state['log']) % len(state['ok'])]
        return ok
    import x86ref
    objs = [x86ref.assemble('WIN32LIB/DRAWBUFF/STAMP.ASM'), x86ref.assemble('WIN32LIB/DRAWBUFF/STMPCACH.ASM')]
    # The stubs' addresses are fixed (STUB_BASE + 16 * n, in stub() order) so
    # the objects can be linked against them before the stubs are installed.
    sets_addr, lookup_addr = 0x00E00000, 0x00E10000
    m = Machine(objs, {'iconsetlist': sets_addr, 'iconcachelookup': lookup_addr,
                       'get_free_cache_slot_': x86ref.STUB_BASE, 'cache_new_icon_': x86ref.STUB_BASE + 16})
    m.stub('get_free_cache_slot_', free_slot)
    m.stub('cache_new_icon_', new_icon)
    for _ in range(n):
        m.reset_heap()
        m.write(m.sym['lasticonset'], b'\0' * 4)
        iw, ih, cnt, iseed = 24, 24, r.randint(1, 60), r.getrandbits(32)
        ic = m.put(iconset(iw, ih, cnt, iseed))
        state.update(ic=ic, n=0, log=[], slots=[r.choice([r.randint(0, 299), -1, r.randint(-9, -2)]) for _ in range(3)],
                     ok=[r.choice([1, 1, 0]) for _ in range(3)])
        regidx = r.choice([-1, r.randint(0, 99)])         # which IconSetList entry holds this set, if any
        lseed = r.getrandbits(32)
        lookup = bytearray(fill(lseed, 6000))
        for i in range(0, 6000, 2):
            if lookup[i] & 1: lookup[i] = lookup[i + 1] = 0xFF   # about half the entries say "not cached"
        m.write(lookup_addr, bytes(lookup))
        sets = bytearray(800)
        listoff = r.randint(0, 2000) * 2
        if regidx >= 0:
            struct.pack_into('<II', sets, regidx * 8, ic, listoff)
        m.write(sets_addr, bytes(sets))
        icon = abs(icon_number(r, iw, ih, cnt))         # a negative one passes the SIGNED range check
                                                         # and indexes before IconCacheLookup in both
        ret = m.call('is_icon_cached_', regs={UC_X86_REG_EAX: ic, UC_X86_REG_EDX: icon & 0xFFFFFFFF})
        log = [x for pair in state['log'] for x in pair]
        yield ([cnt, iseed, regidx, listoff, lseed, icon] + state['slots'] + state['ok']), \
              [s32(ret), fnv(m.read(lookup_addr, 6000)), len(state['log'])] + log


def gen_cache_copy_icon(r, n):
    from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EBX
    m = Machine.from_asm('WIN32LIB/DRAWBUFF/STAMP.ASM', 'WIN32LIB/DRAWBUFF/STMPCACH.ASM',
                         externs={'iconsetlist': 0x00E00000, 'iconcachelookup': 0x00E10000,
                                  'get_free_cache_slot_': 0x0E000000, 'cache_new_icon_': 0x0E000010})
    for _ in range(n):
        m.reset_heap()
        sseed, pitch, dseed = r.getrandbits(32), r.randint(24, 80), r.getrandbits(32)
        src = m.put(fill(sseed, 576))
        dsize = GUARD + pitch * 24 + GUARD
        dbase = m.put(fill(dseed, dsize))
        m.call('cache_copy_icon_', regs={UC_X86_REG_EAX: src, UC_X86_REG_EDX: dbase + GUARD, UC_X86_REG_EBX: pitch})
        yield [sseed, pitch, dseed], [fnv(m.read(dbase, dsize))]


def wild32(r, lim):
    return r.choice([r.randint(-lim, lim), r.randint(-lim, lim), r.randint(-2**31, 2**31 - 1), 0, -1, lim])


def gen_clip_rect(r, n):
    m = Machine.from_asm('WIN32LIB/MISC/CLIPRECT.ASM')
    for _ in range(n):
        m.reset_heap()
        W, H = r.randint(0, 400), r.randint(0, 300)
        vals = [wild32(r, 500) for _ in range(4)]
        ptrs = [m.put(struct.pack('<i', v)) for v in vals]
        ret = m.call('clip_rect', *ptrs, W, H)
        after = [struct.unpack('<i', m.read(p, 4))[0] for p in ptrs]
        yield vals + [W, H], [s32(ret)] + after


def gen_confine_rect(r, n):
    m = Machine.from_asm('WIN32LIB/MISC/CLIPRECT.ASM')
    for _ in range(n):
        m.reset_heap()
        W, H = r.randint(0, 400), r.randint(0, 300)
        x, y, w, h = (wild32(r, 500) for _ in range(4))
        px, py = m.put(struct.pack('<i', x)), m.put(struct.pack('<i', y))
        ret = m.call('confine_rect', px, py, w, h, W, H)
        yield [x, y, w, h, W, H], [s32(ret), struct.unpack('<i', m.read(px, 4))[0], struct.unpack('<i', m.read(py, 4))[0]]


def gen_reverse(r, n):
    m = Machine.from_asm('WIN32LIB/MISC/REVERSE.ASM')
    for _ in range(n):
        v = r.getrandbits(32)
        yield [v], [m.call('reverse_long', v), m.call('reverse_short', v) & 0xFFFF, m.call('swap_long', v)]


def gen_mem_copy(r, n):
    m = Machine.from_asm('WIN32LIB/MEM/MEM_COPY.ASM')
    for _ in range(n):
        m.reset_heap()
        size, seed = r.randint(64, 600), r.getrandbits(32)
        base = m.put(fill(seed, size))
        count = r.choice([r.randint(0, 13), r.randint(14, 40), r.randint(0, size // 2)])
        so = r.randint(0, size - count); do = r.randint(0, size - count)
        if r.random() < 0.4: do = max(0, min(size - count, so + r.randint(-5, 5)))
        nulls = r.choice([0, 0, 0, 0, 1, 2])
        src = 0 if nulls == 1 else base + so
        dst = 0 if nulls == 2 else base + do
        m.call('mem_copy', src, dst, count)
        yield [size, seed, so, do, count, nulls], [fnv(m.read(base, size))]


def gen_set_font_palette_range(r, n):
    m = Machine.from_asm('WIN32LIB/FONT/SETFPAL.ASM', externs={'colorxlat': 0x00E00000})
    for _ in range(n):
        tseed, pseed = r.getrandbits(32), r.getrandbits(32)
        m.write(0x00E00000, fill(tseed, 256))
        pal = m.put(fill(pseed, 16))
        s, e = r.randint(-20, 40), r.randint(-20, 40)
        m.call('set_font_palette_range', pal, s, e)
        yield [tseed, pseed, s, e], [fnv(m.read(0x00E00000, 256))]


def palette(r, seed):
    """768 bytes: usually 6-bit VGA components, sometimes full 8-bit."""
    pal = bytearray(fill(seed, 768))
    if seed & 3:
        pal = bytearray(b & 63 for b in pal)
    return bytes(pal)


def gen_build_fading_table(r, n):
    m = Machine.from_asm('WIN32LIB/MISC/FADING.ASM')
    for _ in range(n):
        m.reset_heap()
        pseed, dseed = r.getrandbits(32), r.getrandbits(32)
        pal = m.put(palette(r, pseed))
        dest = m.put(fill(dseed, 256 + 2 * GUARD))
        color, frac = r.randint(0, 255), r.choice([r.randint(0, 255), r.randint(256, 1000), 0, 255])
        ret = m.call('build_fading_table', pal, dest + GUARD, color, frac)
        yield [pseed, dseed, color, frac], [int(ret == dest + GUARD), fnv(m.read(dest, 256 + 2 * GUARD))]


def gen_bump_color(r, n):
    m = Machine.from_asm('WIN32LIB/PALETTE/PAL.ASM', externs={'set_dd_palette_': 0x0E000000})
    for _ in range(n):
        m.reset_heap()
        pseed = r.getrandbits(32)
        pal = m.put(palette(r, pseed))
        a, b = r.randint(0, 255), r.randint(0, 255)
        if r.random() < 0.1: b = a
        ret = m.call('bump_color', pal, a, b)
        yield [pseed, a, b], [ret, fnv(m.read(pal, 768))]


def gen_set_palette_range(r, n):
    from unicorn.x86_const import UC_X86_REG_EAX
    import x86ref
    m = Machine.from_asm('WIN32LIB/PALETTE/PAL.ASM', externs={'set_dd_palette_': x86ref.STUB_BASE})
    seen = []
    m.stub('set_dd_palette_', lambda mm, _: seen.append(mm.uc.reg_read(UC_X86_REG_EAX)))
    cur = m.sym['currentpalette']
    for i in range(n):
        m.reset_heap()
        seen.clear()
        pseed = r.getrandbits(32)
        pal = m.put(palette(r, pseed))
        before = fnv(m.read(cur, 768)) if i == 0 else 0     # first case: CurrentPalette's initial contents
        m.call('set_palette_range', pal)
        yield [pseed, int(i == 0)], [before, fnv(m.read(cur, 768)), int(seen == [pal])]


def make_font(seed, maxh):
    """A 4-bit proportional font. Mirrored by make_font() in port/tests/asm_game.cpp.
    Glyph widths are 1..12: a zero-width glyph draws 256 pixels a row (8-bit
    counter) and would run off any test view."""
    rnd = fill(seed, 768)
    info, widths, heights, offsets, data = 16, 24, 280, 792, 1304
    w = [1 + rnd[c] % 12 for c in range(256)]
    top = [rnd[256 + c] % (maxh + 1) for c in range(256)]
    ch = [rnd[512 + c] % (maxh - top[c] + 1) for c in range(256)]
    offs, pos = [], data
    for c in range(256):
        offs.append(pos)
        pos += ((w[c] + 1) // 2) * ch[c]
    font = bytearray(pos)
    struct.pack_into('<HHHHHHH', font, 0, pos, 0, info, offsets, widths, data, heights)
    font[info + 4] = maxh
    font[info + 5] = 12
    for c in range(256):
        font[widths + c] = w[c]
        font[heights + 2 * c] = top[c]
        font[heights + 2 * c + 1] = ch[c]
        struct.pack_into('<H', font, offsets + 2 * c, offs[c])
    font[data:] = fill(seed ^ 0xA5A5A5A5, pos - data)
    return bytes(font)


def gen_buffer_print(r, n):
    FONTPTR, XSP, YSP = 0x00E00000, 0x00E00004, 0x00E00008
    m = Machine.from_asm('CODE/2TXTPRNT.ASM', externs={'fontptr': FONTPTR, 'fontxspacing': XSP, 'fontyspacing': YSP})
    xlat = m.sym['colorxlat']
    for i in range(n):
        m.reset_heap()
        v = rand_view(m, r, wmax=r.choice([40, 160, 320]), hmax=r.choice([20, 80, 160]))
        fseed, maxh = r.getrandbits(32), r.randint(1, 16)
        font = 0 if r.random() < 0.02 else m.put(make_font(fseed, maxh))
        xs, ys = r.choice([0, 0, 1, 2, -1]), r.choice([0, 0, 1, 3, -1])
        m.write(FONTPTR, struct.pack('<Iii', font, xs, ys))
        text = bytes(r.choice([10, 13] if r.random() < 0.06 else [r.randint(1, 255) if r.random() < 0.3 else r.randint(32, 126)])
                     for _ in range(r.randint(0, r.choice([8, 30]))))
        sp = m.put(text + b'\0')
        x = r.randint(0, v.w)
        y = r.randint(0, max(0, v.h - maxh + 2)) if r.random() < 0.3 else r.randint(0, max(0, v.h // 4))
        fc, bc = r.getrandbits(8), r.choice([0, 0, r.getrandbits(8)])
        first = fnv(m.read(xlat, 241)) if i == 0 else 0      # the table's initial contents
        if maxh + ys <= 0:
            ys = 0                                          # a line advance of 0 or less: a forced wrap
            m.write(YSP, struct.pack('<i', ys))             # then never ends -- the original hangs
        ret = m.call('buffer_print', v.gvp, sp, x, y, fc, bc)
        rel = ret - v.offset if ret else -(1 << 40)
        yield (v.params() + [fseed if font else 0, maxh, xs, ys, x, y, fc, bc, int(i == 0)] + [len(text)] + list(text)), \
              [rel, v.hash(), fnv(m.read(xlat, 241)), first]


def winasm_machine():
    TABLE = 0x00E00000
    m = Machine.from_asm('CODE/WINASM.ASM', externs={'paletteinterpolationtable': TABLE, 'interpolationpalette': 0x00F00000})
    return m, TABLE


def gen_asm_interpolate(r, n):
    """All three copy types. The source has one extra line: type 2 reads it."""
    m, TABLE = winasm_machine()
    names = ['asm_interpolate', 'asm_interpolate_line_double', 'asm_interpolate_line_interpolate']
    for _ in range(n):
        m.reset_heap()
        kind = r.randint(0, 2)
        tseed, sseed, dseed = r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)
        m.write(TABLE, fill(tseed, 65536))
        w = r.choice([r.randint(4, 40), r.randint(4, 40) * 2, 320])
        h = r.randint(2, 12)
        pitch = 2 * w + r.randint(0, 9)                     # dest line pitch; the caller passes twice it
        src = m.put(fill(sseed, w * (h + 1)))
        dsize = GUARD + pitch * 2 * (h + 1) + GUARD
        dbase = m.put(fill(dseed, dsize))
        m.call(names[kind], src, dbase + GUARD, h, w, 2 * pitch if kind else pitch)
        yield [kind, tseed, sseed, dseed, w, h, pitch], [fnv(m.read(dbase, dsize))]


def gen_modex_blit(r, n):
    """Run ModeX_Blit_ with the VGA modelled -- the sequencer's map mask (port
    3C4h index 2) selecting which of four 64K planes a write at A0000h reaches
    -- and record the screen it produced, read back the mode X way (pixel x of
    a line is plane x & 3, byte x >> 2). The vector holds that screen's hash and
    the source's: equal means the native copy is the faithful replacement.

    One latent bug in the original: each output dword is built with
    `or edx,ecx` after setting only CL/CH, so the UPPER half of ECX on entry
    -- never set by ModeX_Blit_ -- lands in the pixels. The screen equals the
    source only when that half is zero, which is what the intended (and
    native) copy gives; both cases are recorded (garbage_ecx)."""
    from unicorn import UC_HOOK_INSN, UC_HOOK_MEM_WRITE
    from unicorn.x86_const import UC_X86_INS_OUT, UC_X86_REG_EAX, UC_X86_REG_ECX
    m, TABLE = winasm_machine()
    m.uc.mem_map(0xA0000, 0x10000)
    planes = [bytearray(0x10000) for _ in range(4)]
    vga = {'index': 0, 'mask': 0xF}
    def out(uc, port, size, value, _):
        if port == 0x3C4:
            vga['index'] = value & 0xFF
            if size == 2 and vga['index'] == 2: vga['mask'] = (value >> 8) & 0xF
        elif port == 0x3C5 and vga['index'] == 2:
            vga['mask'] = value & 0xF
    def write(uc, access, addr, size, value, _):
        if 0xA0000 <= addr < 0xB0000:
            for k in range(size):
                for p in range(4):
                    if vga['mask'] & (1 << p): planes[p][addr - 0xA0000 + k] = (value >> (8 * k)) & 0xFF
    m.uc.hook_add(UC_HOOK_INSN, out, None, 1, 0, UC_X86_INS_OUT)
    m.uc.hook_add(UC_HOOK_MEM_WRITE, write, begin=0xA0000, end=0xAFFFF)
    for _ in range(n):
        m.reset_heap()
        xadd, pitch, seed = r.choice([0, r.randint(1, 40)]), r.choice([0, r.randint(1, 20)]), r.getrandbits(32)
        stride = 320 + xadd + pitch
        buf = m.put(fill(seed, stride * 200))
        gvp = m.put(struct.pack('<8I', buf, 320, 200, xadd, 0, 0, pitch, 0))
        for p in planes: p[:] = bytes(0x10000)
        garbage = int(r.random() < 0.25)
        m.call('modex_blit_', regs={UC_X86_REG_EAX: gvp, UC_X86_REG_ECX: 0xDEAD0003 if garbage else 0})
        screen = bytes(planes[x & 3][y * 80 + (x >> 2)] for y in range(200) for x in range(320))
        image = b''.join(m.read(buf + y * stride, 320) for y in range(200))
        yield [xadd, pitch, seed, garbage], [fnv(screen), fnv(image)]


def xor_delta(r, limit):
    """A valid format-40 delta touching at most `limit` bytes, using every form."""
    out, pos = bytearray(), 0
    while pos < limit and len(out) < 400 and r.random() < 0.95:
        room = limit - pos
        k = r.randint(0, 5)
        if k == 0:                                           # dump n
            n = r.randint(1, min(127, room)); out += bytes([n]) + fill(r.getrandbits(32), n)
        elif k == 1:                                         # run: 0, n, v
            n = r.randint(1, min(255, room)); out += bytes([0, n, r.getrandbits(8)])
        elif k == 2:                                         # short skip
            n = r.randint(1, min(127, room)); out += bytes([0x80 + n])
        elif k == 3:                                         # long skip
            n = r.randint(1, min(0x7FFF, room)); out += bytes([0x80]) + struct.pack('<H', n)
        elif k == 4:                                         # long dump
            n = r.randint(1, min(0x3FFF, room, 300)); out += bytes([0x80]) + struct.pack('<H', 0x8000 + n) + fill(r.getrandbits(32), n)
        else:                                                # long run
            n = r.randint(1, min(0x3FFF, room)); out += bytes([0x80]) + struct.pack('<H', 0xC000 + n) + bytes([r.getrandbits(8)])
        pos += n
    return bytes(out + b'\x80\x00\x00')


def gen_apply_xor_delta(r, n):
    m = Machine.from_asm('WIN32LIB/WSA/XORDELTA.ASM')
    for _ in range(n):
        m.reset_heap()
        size, seed = r.randint(1, 3000), r.getrandbits(32)
        delta = xor_delta(r, size)
        base = m.put(fill(seed, size + 2 * GUARD))
        dp = m.put(delta)
        ret = m.call('apply_xor_delta', base + GUARD, dp)
        yield [size, seed, len(delta)] + list(delta), [ret, fnv(m.read(base, size + 2 * GUARD))]


def gen_apply_xor_delta_to_page_or_viewport(r, n):
    m = Machine.from_asm('WIN32LIB/WSA/XORDELTA.ASM')
    for _ in range(n):
        m.reset_heap()
        width, rows = r.randint(1, 80), r.randint(1, 40)
        nextrow = width + r.choice([0, 0, r.randint(1, 30)])
        seed, copy = r.getrandbits(32), r.choice([0, 1, 2])
        delta = xor_delta(r, width * rows)
        size = nextrow * rows + 2 * GUARD
        base = m.put(fill(seed, size))
        dp = m.put(delta)
        m.call('apply_xor_delta_to_page_or_viewport', base + GUARD, dp, width, nextrow, copy)
        yield [width, rows, nextrow, seed, copy, len(delta)] + list(delta), [fnv(m.read(base, size))]


def lcw_data(seed, n):
    """Compressible test data: random bytes overwritten with runs, copies of
    earlier stretches and low-entropy patches. Mirrored in asm_game.cpp."""
    d = bytearray(fill(seed, n))
    ctl = fill(seed ^ 0x13579BDF, 128)
    for j in range(0, 128, 4):
        a, b, c, kind = ctl[j], ctl[j + 1], ctl[j + 2], ctl[j + 3]
        pos = a * n // 256
        length = min(n - pos, (b * c) % 400 + 1)
        if kind % 3 == 0:
            for i in range(length): d[pos + i] = b
        elif kind % 3 == 1:
            src = c * n // 256
            for i in range(length):
                if src + i < n: d[pos + i] = d[src + i]
        else:
            for i in range(length): d[pos + i] &= 3
    return bytes(d)


def gen_lcw_comp(r, n):
    m = Machine.from_asm('CODE/LCWCOMP.ASM')
    for _ in range(n):
        m.reset_heap()
        size = r.choice([r.randint(2, 64), r.randint(2, 2000), r.randint(2000, 9000)])
        seed = r.getrandbits(32)
        data = lcw_data(seed, size)
        src = m.put(data + fill(seed ^ 0xFFFFFFFF, 128))     # slack: the assembly peeks past the end
        cap = 2 * size + 256
        dst = m.put(bytes(cap))
        ret = m.call('lcw_comp', src, dst, size)
        yield [size, seed], [ret, fnv(m.read(dst, ret))]


def gen_lcw_uncomp(r, n):
    """LCW_Uncompress, both shipped copies (WIN32LIB/IFF and WINVQ/VQM32), which
    must agree. Streams come from the original compressor (CODE/LCWCOMP.ASM).
    Modes: 0 exact length; 1 a shorter length (every copy/run clamped);
    2 end marker stripped, data ending exactly at the end of its buffer;
    3 a longer length (stops at the end marker)."""
    comp = Machine.from_asm('CODE/LCWCOMP.ASM')
    decs = [Machine.from_asm('WIN32LIB/IFF/LCWUNCMP.ASM'), Machine.from_asm('WINVQ/VQM32/LCWUNCMP.ASM')]
    for _ in range(n):
        size = r.choice([r.randint(1, 64), r.randint(1, 2000), r.randint(2000, 9000)])
        seed = r.getrandbits(32)
        mode = r.randint(0, 3)
        data = lcw_data(seed, size)
        comp.reset_heap()
        src = comp.put(data + fill(seed ^ 0xFFFFFFFF, 128))
        dst = comp.put(bytes(2 * size + 256))
        clen = comp.call('lcw_comp', src, dst, size)
        stream = comp.read(dst, clen)
        if mode == 2 and stream[-1:] == b'\x80':
            stream = stream[:-1]
        length = {0: size, 1: r.randint(0, size), 2: size, 3: size + r.randint(1, 300)}[mode]
        dseed = r.getrandbits(32)
        dsize = max(length, size) + 2 * GUARD
        results = []
        for m in decs:
            m.reset_heap()
            s = m.put(stream + fill(dseed ^ 0x5A5A5A5A, 64))   # what lies past the stream
            d = m.put(fill(dseed, dsize))
            ret = m.call('lcw_uncompress', s, d + GUARD, length)
            results.append([ret, fnv(m.read(d, dsize))])
        assert results[0] == results[1], 'the two shipped LCW_Uncompress copies disagree'
        yield [size, seed, mode, length, dseed, len(stream)], results[0]


def snd1_stream(r):
    """A random SND1 stream; returns (bytes, samples it decodes to)."""
    out, samples = bytearray(), 0
    for _ in range(r.randint(1, 60)):
        code, n = r.randint(0, 3), r.randint(0, 63)
        if code == 2 and r.random() < 0.5:
            n |= 0x20
        if code == 2 and not n & 0x20:
            n &= 0x1F
        out.append(code << 6 | n)
        if code == 0:   out += fill(r.getrandbits(32), n + 1); samples += 4 * (n + 1)
        elif code == 1: out += fill(r.getrandbits(32), n + 1); samples += 2 * (n + 1)
        elif code == 2: (out.__iadd__(fill(r.getrandbits(32), n + 1)) if not n & 0x20 else None); samples += 1 if n & 0x20 else n + 1
        else:           samples += n + 1
    return bytes(out), samples


def _gen_snd1(r, n, asm, name):
    m = Machine.from_asm(asm)
    for _ in range(n):
        m.reset_heap()
        data, samples = snd1_stream(r)
        count = r.choice([samples, samples, r.randint(1, samples)])
        src = m.put(data + bytes(64))
        dseed = r.getrandbits(32)
        dsize = samples + 2 * GUARD
        dst = m.put(fill(dseed, dsize))
        ret = m.call(name, src, dst + GUARD, count)
        yield [count, dseed, samples, len(data)] + list(data), [s32(ret), fnv(m.read(dst, dsize))]


def gen_decompress_frame(r, n):
    yield from _gen_snd1(r, n, 'WIN32LIB/AUDIO/AUDUNCMP.ASM', 'decompress_frame')


def gen_audio_unzap(r, n):
    yield from _gen_snd1(r, n, 'WINVQ/VQM32/AUDUNZAP.ASM', 'audiounzap')


def asm_struct(path, name):
    """{field: (offset, size)} of a STRUC, as tasm2gas parsed the file."""
    import tasm2gas
    c = tasm2gas.Converter(tasm2gas.makefile_defines(os.path.join(ROOT, path)))
    c.convert(os.path.join(ROOT, path))
    return c.structs[name.lower()]


SOS_FIELDS = ['dwsampleindex', 'dwpredicted', 'dwdifference', 'wcodebuf', 'wcode', 'wstep', 'windex',
              'dwsampleindex2', 'dwpredicted2', 'dwdifference2', 'wcodebuf2', 'wcode2', 'wstep2', 'windex2']


def _gen_sos(r, n, asm, decomp, init, fixed_format=None):
    """Chunked decoding, state carried in the struct between calls. Inputs:
    format, start state (or init), chunk sizes, data seed. Outputs: output hash
    and the struct's final state."""
    m = Machine.from_asm(asm)
    size, fields = asm_struct(asm, 'sCompInfo')
    for _ in range(n):
        m.reset_heap()
        bits, chans = fixed_format or (r.choice([8, 16]), r.choice([1, 2]))
        nchunks = r.randint(1, 4)
        unit = (2 if bits == 16 else 1) * (2 if chans == 2 else 1)
        chunks = [unit * r.randint(1, 200) for _ in range(nchunks)]
        seed = r.getrandbits(32)
        total_out = sum(chunks)
        src_len = total_out // (2 if bits == 16 else 1) // 2 + 4
        src = m.put(fill(seed, src_len + 64))
        dseed = r.getrandbits(32)
        dst = m.put(fill(dseed, total_out + 2 * GUARD))
        info = m.put(bytes(size))
        def put(f, v):
            off, sz = fields[f]
            m.write(info + off, struct.pack({2: '<h', 4: '<i'}[sz], v))
        def get(f):
            off, sz = fields[f]
            return struct.unpack({2: '<h', 4: '<i'}[sz], m.read(info + off, sz))[0]
        put('wbitsize', bits); put('wchannels', chans)
        use_init = bool(init) and (fixed_format is not None or r.random() < 0.5)
        if use_init:
            m.call(init, info)
            start = [0] * 6
        else:
            i1, i2 = r.randint(0, 88), r.randint(0, 88)
            start = [r.randint(-32768, 32767), i1, r.randint(1, 32767), r.randint(-32768, 32767), i2, r.randint(1, 32767)]
            put('dwpredicted', start[0]); put('windex', start[1]); put('wstep', start[2])
            put('dwpredicted2', start[3]); put('windex2', start[4]); put('wstep2', start[5])
        sp, dp = src, dst + GUARD
        rets = []
        for c in chunks:
            m.write(info + fields['lpsource'][0], struct.pack('<I', sp))
            m.write(info + fields['lpdest'][0], struct.pack('<I', dp))
            rets.append(m.call(decomp, info, c))
            sp += c // (2 if bits == 16 else 1) // 2
            dp += c
        state = [get(f) for f in ('dwpredicted', 'windex', 'wstep', 'dwpredicted2', 'windex2', 'wstep2')]
        yield ([bits, chans, int(use_init)] + start + [seed, dseed, len(chunks)] + chunks), \
              [fnv(m.read(dst, total_out + 2 * GUARD))] + state + rets


def gen_general_sos(r, n):
    yield from _gen_sos(r, n, 'WIN32LIB/AUDIO/OLSOSDEC.ASM', 'general_soscodecdecompressdata', 'soscodecinitstream')


def gen_sos16(r, n):
    """The library's table-driven 16-bit mono decoder, from its own InitStream:
    the state it keeps is not the step-table form, so only output is compared."""
    yield from _gen_sos(r, n, 'WIN32LIB/AUDIO/SOSCODEC.ASM', 'soscodecdecompressdata', 'soscodecinitstream', (16, 1))


def gen_vqa_sos(r, n):
    yield from _gen_sos(r, n, 'WINVQ/VQM32/SOSCODEC.ASM', 'vqa_soscodecdecompressdata', 'vqa_soscodecinitstream')


def gen_unvq_4x2(r, n):
    m = Machine.from_asm('WINVQ/VQA32/UNVQBUFF.ASM')
    for _ in range(n):
        m.reset_heap()
        bpr, rows = r.randint(1, 40), r.randint(1, 30)
        bufwidth = 4 * bpr + r.choice([0, 0, r.randint(1, 20)])
        cbentries = r.randint(1, 3840)
        cseed, pseed, dseed = r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)
        entries = bpr * rows
        ptr = bytearray(fill(pseed, 2 * entries))
        for i in range(entries):                      # valid codebook indices, plenty of 0x0F fills
            if ptr[entries + i] & 3 == 0:
                ptr[entries + i] = 0x0F
            else:
                idx = (ptr[entries + i] << 8 | ptr[i]) % cbentries
                if idx >> 8 == 0x0F: idx = 0
                ptr[i], ptr[entries + i] = idx & 0xFF, idx >> 8
        cb = m.put(fill(cseed, 8 * cbentries))
        pp = m.put(bytes(ptr))
        size = GUARD + bufwidth * 2 * rows + GUARD
        buf = m.put(fill(dseed, size))
        m.call('unvq_4x2', cb, pp, buf + GUARD, bpr, rows, bufwidth)
        yield [bpr, rows, bufwidth, cbentries, cseed, pseed, dseed], [fnv(m.read(buf, size))]


SH_CENTER, SH_TRANS, SH_FADING, SH_PRED, SH_GHOST, SH_PARTIAL = 0x20, 0x40, 0x100, 0x200, 0x1000, 0x4000


def frame_tables(seed, nrows):
    """Ghost table (IsTranslucent[256] + 256 rows of 256) and a fading table.
    Mirrored in asm_game.cpp."""
    ist = bytearray(fill(seed, 256))
    for i in range(256):
        ist[i] = 0xFF if ist[i] & 1 else ist[i] % nrows
    rows = fill(seed ^ 0x2468ACE0, 256 * 256)
    fadet = fill(seed ^ 0x0F0F0F0F, 256)
    return bytes(ist) + rows, fadet


def frame_pixels(seed, w, h):
    px = bytearray(fill(seed, w * h))
    for i in range(len(px)):
        if px[i] % 3 == 0: px[i] = 0                     # plenty of transparent pixels
    for row in range(h):                                 # some all-transparent lines (BLIT_SKIP)
        if px[row * w] == 7:
            px[row * w:(row + 1) * w] = bytes(w)
    return bytes(px)


def gen_buffer_frame_to_page(r, n):
    BIG, THEATER, USEBIG, USEOLD = 0x00E00000, 0x00E00004, 0x00E00008, 0x00E0000C
    m = Machine.from_asm('CODE/2KEYFBUF.ASM', externs={
        'bigshapebufferstart': BIG, 'theatershapebufferstart': THEATER,
        'usebigshapebuffer': USEBIG, 'useoldshapedraw': USEOLD, 'mmxavailable': 0x00E00010})
    for _ in range(n):
        m.reset_heap()
        # The shimmer reads a destination pixel up to a line below / 5 right,
        # past the bottom row as the original did; a stride under the 64-byte
        # guard keeps those reads on bytes both sides have.
        # (And a stride of at least 6: the shimmer's "line below, k left"
        # offsets are 16-bit stride - k, which below that wrap to ~65530.)
        v = View(m, r.randint(8, 40), r.randint(1, 40), r.choice([0, 0, r.randint(1, 9)]),
                 r.choice([0, 0, r.randint(1, 7)]), r.randint(0, 3), r.getrandbits(32))
        w, h = r.randint(1, 40), r.randint(1, 30)
        pseed, tseed, nrows = r.getrandbits(32), r.getrandbits(32), r.randint(1, 255)
        pixels = frame_pixels(pseed, w, h)
        ghost, fadet = frame_tables(tseed, nrows)
        gp, fp = m.put(ghost), m.put(fadet)
        usebig, useold = r.choice([0, 1, 1]), r.choice([0, 0, 0, 1])
        theater = r.randint(0, 1)
        # the shape: in the big (or theater) buffer behind a header, or plain
        region = m.alloc(64 + len(pixels) + 16)
        base = m.alloc(16)
        m.write(region + 64, pixels)
        hdr = m.put(struct.pack('<Iii', 0xFFFFFFFF, region + 64 - base, theater) + bytes(h))
        m.write(BIG, struct.pack('<IIII', base if not theater else 0, base if theater else 0, usebig, useold))
        src = hdr if (usebig and not useold) else region + 64
        calls = []
        for k in range(r.choice([1, 1, 2, 3])):
            flags = 0
            for f in (SH_TRANS, SH_GHOST, SH_FADING, SH_PRED, SH_PARTIAL, SH_CENTER):
                if r.random() < 0.4: flags |= f
            if k and r.random() < 0.6: flags = calls[-1][2]   # same flags again: the cached path
            if (flags & SH_PRED) and (flags & SH_GHOST) and (flags & SH_PARTIAL):
                flags &= ~SH_PARTIAL                       # address-dependent in the original (see 2KEYFBUF.CPP)
            if r.random() < 0.7:
                x, y = r.randint(0, max(0, v.w - w)), r.randint(0, max(0, v.h - h))
            else:
                x, y = r.randint(-w, v.w), r.randint(-h, v.h)
            if flags & SH_CENTER:
                x += w >> 1; y += h >> 1
            fcount = r.randint(1, 4)
            pred, partial = r.randint(-8, 8), r.getrandbits(8)
            gsel = r.randint(0, 1)                         # a second ghost table changes what is translucent
            g2 = m.put(frame_tables(tseed ^ 0x55555555, nrows)[0]) if gsel else gp
            args = []
            if flags & SH_GHOST: args.append(g2)
            if flags & SH_FADING: args += [fp, fcount]
            if flags & SH_PRED: args.append(pred & 0xFFFFFFFF)
            if flags & SH_PARTIAL: args.append(partial)
            m.call('buffer_frame_to_page', x & 0xFFFFFFFF, y & 0xFFFFFFFF, w, h, src, v.gvp, flags, *args)
            calls.append((x, y, flags, fcount, pred, partial, gsel))
        flat = [x for c in calls for x in c]
        yield (v.params() + [w, h, pseed, tseed, nrows, usebig, useold, theater, len(calls)] + flat), \
              [v.hash(), fnv(m.read(hdr, 4) + m.read(hdr + 12, h))]   # draw_flags + line classes


ROUTINES = {name[4:]: fn for name, fn in globals().items() if name.startswith('gen_') and callable(fn)}

MOUSE_FIELDS = None


def mouse_struct(m, fields_out, **kw):
    """A MouseType (MOUSE.INC) in emulated memory."""
    global MOUSE_FIELDS
    if MOUSE_FIELDS is None:
        MOUSE_FIELDS = asm_struct('WIN32LIB/KEYBOARD/WWMOUSE.ASM', 'MouseType')
    size, fields = MOUSE_FIELDS
    p = m.put(bytes(size))
    for k, v in kw.items():
        off, sz = fields[k.lower()]
        m.write(p + off, struct.pack({1: '<B', 4: '<I'}[sz], v & 0xFFFFFFFF))
    fields_out.update(fields)
    return p


def mouse_machine():
    import x86ref
    SHAPEBUF = 0x00E00000
    objs = [x86ref.assemble('WIN32LIB/KEYBOARD/WWMOUSE.ASM'), x86ref.assemble('WIN32LIB/IFF/LCWUNCMP.ASM'),
            x86ref.assemble('CODE/LCWCOMP.ASM')]
    m = Machine(objs, {'shapebuffer': SHAPEBUF, 'get_shape_uncomp_size': x86ref.STUB_BASE,
                       'get_shape_width': x86ref.STUB_BASE + 16, 'get_shape_original_height': x86ref.STUB_BASE + 32})
    # GETSHAPE.CPP's readers of the 10-byte Shape_Type header
    m.stub('get_shape_uncomp_size', lambda mm, a: struct.unpack('<H', mm.read(mm.u32(a) + 8, 2))[0])
    m.stub('get_shape_width', lambda mm, a: struct.unpack('<H', mm.read(mm.u32(a) + 3, 2))[0])
    m.stub('get_shape_original_height', lambda mm, a: mm.read(mm.u32(a) + 5, 1)[0])
    return m, SHAPEBUF


def gen_mouse_shadow_buffer(r, n):
    m, _ = mouse_machine()
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r, wmax=64, hmax=48)
        cw, ch = r.randint(1, 40), r.randint(1, 30)
        bseed = r.getrandbits(32)
        buf = m.put(fill(bseed, GUARD + cw * ch + GUARD))
        f = {}
        mp = mouse_struct(m, f, CursorWidth=cw, CursorHeight=ch)
        x, y = r.randint(-cw, v.w + 5), r.randint(-ch, v.h + 5)
        hx, hy, store = r.randint(0, cw - 1), r.randint(0, ch - 1), r.choice([0, 1, 1, 2])
        m.call('mouse_shadow_buffer', mp, v.gvp, buf + GUARD, x, y, hx, hy, store)
        yield v.params() + [cw, ch, bseed, x, y, hx, hy, store], [v.hash(), fnv(m.read(buf, GUARD + cw * ch + GUARD))]


def gen_draw_mouse(r, n):
    m, _ = mouse_machine()
    for _ in range(n):
        m.reset_heap()
        v = rand_view(m, r, wmax=64, hmax=48)
        cw, ch = r.randint(1, 40), r.randint(1, 30)
        cseed = r.getrandbits(32)
        cur = bytearray(fill(cseed, cw * ch))
        for i in range(len(cur)):
            if cur[i] & 1: cur[i] = 0
        cp = m.put(bytes(cur))
        hx, hy = r.randint(0, cw - 1), r.randint(0, ch - 1)
        f = {}
        mp = mouse_struct(m, f, MouseCursor=cp, CursorWidth=cw, CursorHeight=ch, MouseXHot=hx, MouseYHot=hy)
        x, y = r.randint(-cw, v.w + 5), r.randint(-ch, v.h + 5)
        m.call('draw_mouse', mp, v.gvp, x, y)
        yield v.params() + [cw, ch, cseed, hx, hy, x, y], [v.hash()]


def cursor_shape(seed, w, h, stype):
    """A cursor shape: 10-byte header (+16 colours if compact), RLE data,
    LCW-packed after the header when stype lacks MAKESHAPE_NOCOMP (2). The
    packing is done by the caller (with the original LCW_Comp). Returns
    (header bytes, rle bytes). Mirrored in asm_misc.cpp."""
    px = bytearray(fill(seed, w * h))
    rle, i = bytearray(), 0
    while i < len(px):
        if px[i] % 4 == 0:                                  # a transparent run
            run = 1
            while i + run < len(px) and px[i + run] % 4 == 0 and run < 255: run += 1
            rle += bytes([0, run]); i += run
        else:
            rle.append(px[i] if not stype & 1 else (px[i] % 15) + 1); i += 1
    hdr = struct.pack('<HBHBHH', stype, h, w, h, 0, len(rle))
    if stype & 1:
        hdr += fill(seed ^ 0x77777777, 16)
    return hdr, bytes(rle)


def gen_set_mouse_cursor(r, n):
    m, SHAPEBUF = mouse_machine()
    for _ in range(n):
        m.reset_heap()
        stype = r.choice([0, 1, 2, 3])
        maxw, maxh = r.randint(8, 40), r.randint(8, 30)
        w, h = r.randint(1, maxw + 3), r.randint(1, maxh + 3)
        seed = r.getrandbits(32)
        hdr, rle = cursor_shape(seed, w, h, stype)
        if stype & 2:
            shape = hdr + rle
        else:
            sp = m.put(rle + bytes(128))
            dp = m.alloc(2 * len(rle) + 256)
            clen = m.call('lcw_comp', sp, dp, len(rle))
            shape = hdr + m.read(dp, clen)
        sh = m.put(shape + bytes(64))
        m.write(SHAPEBUF, struct.pack('<I', m.alloc(4096)))
        cseed = r.getrandbits(32)
        cur = m.put(fill(cseed, maxw * maxh + 64))
        prev = r.getrandbits(32)
        f = {}
        mp = mouse_struct(m, f, MouseCursor=cur, MaxWidth=maxw, MaxHeight=maxh, PrevCursor=prev,
                          MouseXHot=7, MouseYHot=9, CursorWidth=3, CursorHeight=4)
        hx, hy = r.randint(0, 20), r.randint(0, 20)
        ret = m.call('asm_set_mouse_cursor', mp, hx, hy, sh)
        state = [struct.unpack('<i', m.read(mp + f[k][0], 4))[0] for k in ('mousexhot', 'mouseyhot', 'cursorwidth', 'cursorheight')]
        newprev = m.u32(mp + f['prevcursor'][0])
        yield [stype, maxw, maxh, w, h, seed, cseed, prev, hx, hy], \
              [int(ret == prev), int(newprev == sh)] + state + [fnv(m.read(cur, maxw * maxh + 64))]


# Cases per routine: what port/tests/asm_vectors holds. RA_VECTORS overrides.
COUNTS = {
    'apply_xor_delta': 500, 'apply_xor_delta_to_page_or_viewport': 500, 'asm_interpolate': 400,
    'audio_unzap': 500, 'buffer_clear': 400, 'buffer_draw_line': 1000, 'buffer_draw_stamp': 600,
    'buffer_draw_stamp_clip': 600, 'buffer_fill_rect': 400, 'buffer_frame_to_page': 600,
    'buffer_get_pixel': 400, 'buffer_print': 800, 'buffer_put_pixel': 400, 'buffer_remap': 400,
    'buffer_to_buffer': 400, 'buffer_to_page': 400, 'build_fading_table': 500, 'bump_color': 500,
    'cache_copy_icon': 600, 'clip_rect': 800, 'confine_rect': 800, 'decompress_frame': 500,
    'general_sos': 300, 'is_icon_cached': 600, 'lcw_comp': 300, 'linear_blit_to_linear': 1000,
    'linear_scale_to_linear': 1000, 'mem_copy': 800, 'modex_blit': 24, 'reverse': 800,
    'set_font_palette_range': 800, 'set_palette_range': 500, 'sos16': 300, 'unvq_4x2': 400,
    'vqa_sos': 300, 'lcw_uncomp': 600, 'mouse_shadow_buffer': 600, 'draw_mouse': 600, 'set_mouse_cursor': 400,
}


def main():
    ROUTINES.update({k[4:]: f for k, f in globals().items() if k.startswith('gen_') and callable(f)})
    names = sys.argv[1:] or sorted(ROUTINES)
    os.makedirs(OUT, exist_ok=True)
    for name in names:
        r = random.Random(name)                     # deterministic per routine
        n = int(os.environ.get('RA_VECTORS', COUNTS.get(name, 400)))
        lines = [' '.join(map(str, i)) + ' : ' + ' '.join(map(str, o)) for i, o in ROUTINES[name](r, n)]
        open(os.path.join(OUT, name + '.txt'), 'w').write('\n'.join(lines) + '\n')
        print(f'{name}: {len(lines)} cases from the original assembly')


if __name__ == '__main__':
    main()
