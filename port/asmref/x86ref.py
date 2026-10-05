"""
x86ref.py -- run Westwood's original 32-bit x86 routines as a reference.

    m = Machine.from_asm('WIN32LIB/DRAWBUFF/CLEAR.ASM')
    buf = m.alloc(64 * 48)
    m.call('buffer_clear', view, 7)

Each .ASM is converted by tasm2gas.py, assembled by clang for i386 ELF, then
linked here (a minimal ELF32 relocator: R_386_32 and R_386_PC32 are all clang
emits for this code) into a flat 32-bit address space run by Unicorn.

Symbols are lowercase (see tasm2gas.py). Undefined symbols must be supplied by
the caller -- `externs={'name': address}` -- or the load fails: a reference
routine must never silently jump into nothing.
"""
import os, struct, subprocess, sys, tempfile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UcError, UC_HOOK_CODE
from unicorn.x86_const import *

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import tasm2gas

CODE_BASE = 0x00100000
HEAP_BASE = 0x01000000
HEAP_SIZE = 0x04000000
STACK_TOP = 0x08000000
STACK_SIZE = 0x00100000
RETURN_TRAP = 0x0F000000          # calls return here; mapped, never executed
STUB_BASE = 0x0E000000


def assemble(asm_path, defines=None):
    cache = os.path.join(tempfile.gettempdir(), 'ra-asmref')
    os.makedirs(cache, exist_ok=True)
    stem = os.path.basename(asm_path).rsplit('.', 1)[0]
    s_path, o_path = os.path.join(cache, stem + '.s'), os.path.join(cache, stem + '.o')
    defs = {**tasm2gas.makefile_defines(asm_path), **(defines or {})}
    open(s_path, 'w').write(tasm2gas.Converter(defs).convert(os.path.join(ROOT, asm_path)))
    subprocess.run(['clang', '-target', 'i386-pc-linux-gnu', '-c', s_path, '-o', o_path], check=True)
    return open(o_path, 'rb').read()


class Elf32:
    def __init__(self, data):
        self.d = data
        assert data[:4] == b'\x7fELF' and data[4] == 1, 'not ELF32'
        (self.shoff,) = struct.unpack_from('<I', data, 0x20)
        self.shentsize, self.shnum, self.shstrndx = struct.unpack_from('<HHH', data, 0x2E)
        self.sh = [struct.unpack_from('<IIIIIIIIII', data, self.shoff + i * self.shentsize) for i in range(self.shnum)]
        names = self.sh[self.shstrndx]
        self.names = [self.cstr(names[4] + s[0]) for s in self.sh]

    def cstr(self, off):
        return self.d[off:self.d.index(b'\0', off)].decode()

    def symbols(self):
        for i, s in enumerate(self.sh):
            if s[1] == 2:                                  # SHT_SYMTAB
                strtab = self.sh[s[6]]
                for j in range(s[5] // 16):
                    name, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', self.d, s[4] + j * 16)
                    yield j, self.cstr(strtab[4] + name) if name else '', value, shndx, info >> 4


class Machine:
    def __init__(self, objects, externs=None):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.uc.mem_map(CODE_BASE, 0x00F00000)
        self.uc.mem_map(HEAP_BASE, HEAP_SIZE)
        self.uc.mem_map(STACK_TOP - STACK_SIZE, STACK_SIZE)
        self.uc.mem_map(RETURN_TRAP, 0x1000)
        self.uc.mem_map(STUB_BASE, 0x1000)
        self.heap = HEAP_BASE
        self.sym = {}
        self.stubs = {}
        externs = dict(externs or {})
        place = CODE_BASE
        loaded = []
        for data in objects:
            elf = Elf32(data)
            addr = {}
            for i, s in enumerate(elf.sh):
                if s[1] in (1, 8) and s[2] & 2:            # PROGBITS / NOBITS, SHF_ALLOC
                    place = (place + 15) & ~15
                    addr[i] = place
                    if s[1] == 1:
                        self.uc.mem_write(place, elf.d[s[4]:s[4] + s[5]])
                    place += s[5]
            syms = {}
            for j, name, value, shndx, bind in elf.symbols():
                if shndx and shndx < 0xFF00 and shndx in addr:
                    syms[j] = addr[shndx] + value
                    if name and bind == 1:
                        self.sym[name] = syms[j]
                elif name:
                    syms[j] = name                         # undefined: resolve later
            loaded.append((elf, addr, syms))
        for elf, addr, syms in loaded:
            for i, s in enumerate(elf.sh):
                if s[1] != 9 or s[7] not in addr:          # SHT_REL for an allocated section
                    continue
                base = addr[s[7]]
                for k in range(s[5] // 8):
                    off, info = struct.unpack_from('<II', elf.d, s[4] + k * 8)
                    t, j = info & 0xFF, info >> 8
                    S = syms.get(j)
                    if isinstance(S, str):
                        if S in self.sym:
                            S = self.sym[S]
                        elif S in externs:
                            S = externs[S]
                        else:
                            raise KeyError(f'undefined symbol {S!r}: supply it in externs=')
                    P = base + off
                    A = struct.unpack('<i', self.uc.mem_read(P, 4))[0]
                    if t == 1:   v = S + A
                    elif t == 2: v = S + A - P
                    else: raise ValueError(f'relocation type {t}')
                    self.uc.mem_write(P, struct.pack('<I', v & 0xFFFFFFFF))
        self.uc.hook_add(UC_HOOK_CODE, self._stub_hook, begin=STUB_BASE, end=STUB_BASE + 0xFFF)

    @classmethod
    def from_asm(cls, *paths, externs=None, defines=None):
        return cls([assemble(p, defines) for p in paths], externs)

    # ------------------------------------------------------------- memory
    def alloc(self, size, fill=None, align=16):
        self.heap = (self.heap + align - 1) & ~(align - 1)
        a = self.heap
        self.heap += size
        assert self.heap < HEAP_BASE + HEAP_SIZE, 'reference heap exhausted'
        if fill is not None:
            self.uc.mem_write(a, bytes(fill) if not isinstance(fill, int) else bytes([fill]) * size)
        return a

    def put(self, data, align=16):
        a = self.alloc(len(data), align=align)
        self.uc.mem_write(a, bytes(data))
        return a

    def read(self, addr, n):
        return bytes(self.uc.mem_read(addr, n))

    def write(self, addr, data):
        self.uc.mem_write(addr, bytes(data))

    def u32(self, addr):
        return struct.unpack('<I', self.read(addr, 4))[0]

    def reset_heap(self):
        self.heap = HEAP_BASE

    # -------------------------------------------------------------- calls
    def stub(self, name, fn):
        """An external function, implemented in Python: fn(machine, args_ptr) -> eax."""
        a = STUB_BASE + 16 * len(self.stubs)
        self.uc.mem_write(a, b'\xc3')                      # ret
        self.stubs[a] = fn
        return a

    def _stub_hook(self, uc, address, size, _):
        fn = self.stubs.get(address)
        if fn:
            esp = uc.reg_read(UC_X86_REG_ESP)
            r = fn(self, esp + 4)
            if r is not None:
                uc.reg_write(UC_X86_REG_EAX, r & 0xFFFFFFFF)

    def call(self, name, *args, limit=200_000_000, regs=None):
        """cdecl call; returns EAX (unsigned 32-bit)."""
        entry = self.sym[name] if isinstance(name, str) else name
        esp = STACK_TOP - 256
        frame = b''.join(struct.pack('<I', a & 0xFFFFFFFF) for a in args)
        esp -= len(frame)
        self.uc.mem_write(esp, frame)
        esp -= 4
        self.uc.mem_write(esp, struct.pack('<I', RETURN_TRAP))
        # Garbage in every register: the original must not depend on them.
        for r, v in ((UC_X86_REG_EAX, 0xDEAD0001), (UC_X86_REG_EBX, 0xDEAD0002), (UC_X86_REG_ECX, 0xDEAD0003),
                     (UC_X86_REG_EDX, 0xDEAD0004), (UC_X86_REG_ESI, 0xDEAD0005), (UC_X86_REG_EDI, 0xDEAD0006),
                     (UC_X86_REG_EBP, 0xDEAD0007)):
            self.uc.reg_write(r, v)
        for r, v in (regs or {}).items():
            self.uc.reg_write(r, v)
        self.uc.reg_write(UC_X86_REG_ESP, esp)
        self.uc.reg_write(UC_X86_REG_EFLAGS, 0x202)        # DF clear, as the C ABI guarantees
        try:
            self.uc.emu_start(entry, RETURN_TRAP, count=limit)
        except UcError as e:
            eip = self.uc.reg_read(UC_X86_REG_EIP)
            raise RuntimeError(f'{name}: {e} at eip={eip:#x}') from None
        if self.uc.reg_read(UC_X86_REG_EIP) != RETURN_TRAP:
            raise RuntimeError(f'{name}: did not return within {limit} instructions')
        if self.uc.reg_read(UC_X86_REG_ESP) != esp + 4:
            raise RuntimeError(f'{name}: stack unbalanced on return')
        return self.uc.reg_read(UC_X86_REG_EAX)

    def edx(self):
        return self.uc.reg_read(UC_X86_REG_EDX)
