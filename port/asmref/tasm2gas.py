#!/usr/bin/env python3
"""
tasm2gas.py <file.ASM> [-o out.s] [-D NAME=VALUE ...]

Converts Westwood's TASM IDEAL-mode assembly into GNU-as Intel syntax that
clang's integrated assembler accepts for a 32-bit x86 target, so the ORIGINAL
routines can be executed (port/asmref/harness.py runs them under the Unicorn
CPU emulator) and compared byte-for-byte with their C translations.

This is a test tool, not part of the game. It only has to be faithful for the
constructs these files use; anything it does not recognise is an error rather
than a guess, so a silent mistranslation cannot hide a real difference.

What TASM does implicitly, reproduced here:
  * PROC ... C with ARG/LOCAL: `push ebp / mov ebp,esp / sub esp,locals`, then
    the USES registers; every RET inside the PROC becomes the matching epilogue
    (RETN stays a bare return, for internal subroutines).
  * ARG offsets from [ebp+8], each rounded up to 4 bytes; LOCALs allocated
    downward from [ebp] in declaration order.
  * IDEAL memory operands take their size from the symbol's declared type.
  * In IDEAL mode a bare data label is its address (`offset`); GNU Intel syntax
    would read it as memory, so `offset` is added.
  * Symbols are case-insensitive (everything is lowercased).
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SIZES = {'byte': 1, 'word': 2, 'dword': 4, 'fword': 6, 'qword': 8, 'tbyte': 10, 'near': 4, 'far': 6, 'proc': 4}
PTR = {1: 'byte', 2: 'word', 4: 'dword', 6: 'fword', 8: 'qword', 10: 'tbyte'}
REGS = set('eax ebx ecx edx esi edi ebp esp ax bx cx dx si di bp sp al ah bl bh cl ch dl dh '
           'cs ds es fs gs ss st'.split())
SEGREGS = {'cs', 'ds', 'es', 'ss'}
IGNORE = {'ideal', 'masm', 'p386', 'p486', 'p386n', 'p486n', 'p586', 'model', 'locals', 'jumps', 'nojumps',
          'end', 'smart', 'nosmart', 'option', 'assume', 'radix', 'p387', 'masm51', 'quirks', '.386',
          '.486', '.model', 'title', 'subttl', 'page', '%title', 'display', '%out', 'nowarn', 'warn',
          'pnowarn', 'extrn', 'extern', 'public', 'publicdll', 'codeseg', 'dataseg', 'udataseg',
          'const', '.code', '.data', 'ends', 'segment', 'group'}


class AsmError(Exception):
    pass


def num(tok):
    """TASM numeric literal -> int, or None."""
    t = tok.lower()
    if re.fullmatch(r'[0-9][0-9a-f]*h', t):
        return int(t[:-1], 16)
    if re.fullmatch(r'[01]+b', t):
        return int(t[:-1], 2)
    if re.fullmatch(r'[0-7]+[oq]', t):
        return int(t[:-1], 8)
    if re.fullmatch(r'[0-9]+d?', t):
        return int(t.rstrip('d'))
    return None


def strip_comment(line):
    out, q = [], None
    for ch in line:
        if q:
            out.append(ch)
            if ch == q:
                q = None
        elif ch in '\'"':
            q = ch; out.append(ch)
        elif ch == ';':
            break
        else:
            out.append(ch)
    return ''.join(out).rstrip()


def ci_path(base, rel):
    """Resolve rel under base, matching each component case-insensitively."""
    cur = base
    for part in rel.split('/'):
        if part in ('', '.'):
            continue
        if not os.path.isdir(cur):
            return None
        hit = [f for f in os.listdir(cur) if f.lower() == part.lower()]
        if not hit:
            return None
        cur = os.path.join(cur, hit[0])
    return cur if os.path.isfile(cur) else None


def find_include(name, here):
    name = name.strip('"\'<>').replace('\\', '/')
    for d in (here, os.path.join(ROOT, 'WIN32LIB/INCLUDE'), os.path.join(ROOT, 'WINVQ/INCLUDE'),
              os.path.join(ROOT, 'WINVQ/INCLUDE/VQM32'), os.path.join(ROOT, 'WINVQ/INCLUDE/VQA32'),
              os.path.join(ROOT, 'CODE')):
        f = ci_path(d, name)
        if f:
            return f
    raise AsmError(f'include not found: {name}')


class Converter:
    def __init__(self, defines=None):
        self.equ = {}            # name -> text (lowercase)
        self.structs = {}        # name -> (size, {field: (offset, size)})
        self.syms = {}           # data label -> size of one element
        self.code_labels = set()
        self.macros = {}         # name -> (params, body lines)
        self.out = []
        self.proc = None
        self.cur_struct = None
        self.section = None
        self.globals = set()
        for k, v in (defines or {}).items():
            self.equ[k.lower()] = str(v)

    # ------------------------------------------------------------------ input
    def lines_of(self, path):
        here = os.path.dirname(path)
        for raw in open(path, encoding='latin-1'):
            ln = strip_comment(raw.rstrip('\n').replace('\t', ' ')).strip().strip('\x1a\x0c').strip()
            if re.fullmatch(r'(?i)end(\s+\w+)?', ln):
                return                                  # TASM stops reading at END
            m = re.match(r'(?i)include\s+(\S+)', ln)
            if m:
                yield from self.lines_of(find_include(m.group(1), here))
            else:
                yield ln

    # ------------------------------------------------------------ expressions
    def subst_equ(self, s, depth=0):
        if depth > 20:
            raise AsmError('equ recursion: ' + s)
        def rep(m):
            w = m.group(0)
            return '(' + self.equ[w] + ')' if w in self.equ and re.fullmatch(r'[-+*/()0-9a-fhx\s<>]+', self.equ[w] or '') \
                else self.equ.get(w, w)
        new = re.sub(r'(?<![\w?@.])[a-z_?@][\w?@]*', rep, s)
        return new if new == s else self.subst_equ(new, depth + 1)

    def eval(self, s):
        """Evaluate a constant expression (equates, struct sizes, numbers)."""
        s = self.subst_equ(s.lower())
        s = re.sub(r'\bsize\s+(\w+)', lambda m: str(self.size_of(m.group(1))), s)
        s = re.sub(r'\b[0-9][0-9a-f]*[hboqd]?\b', lambda m: str(num(m.group(0))) if num(m.group(0)) is not None else m.group(0), s)
        s = re.sub(r'(\w+)\.(\w+)', lambda m: str(self.field(m.group(1), m.group(2))[0]) if m.group(1) in self.structs else m.group(0), s)
        s = re.sub(r'\bshl\b', '<<', s); s = re.sub(r'\bshr\b', '>>', s)
        s = re.sub(r'\band\b', '&', s); s = re.sub(r'\bor\b', '|', s); s = re.sub(r'\bxor\b', '^', s)
        s = re.sub(r'\bnot\b', '~', s); s = re.sub(r'\bmod\b', '%', s)
        s = s.replace('/', '//')
        if not re.fullmatch(r'[-+*/%()0-9\s<>&|^~]*', s):
            raise AsmError('not a constant: ' + s)
        return eval(s)

    def size_of(self, t):
        t = t.lower().strip()
        if re.fullmatch(r'(near\s+|far\s+)?ptr(\s+\w+)?|(near|far)\s+ptr\s+\w+', t) or t.startswith('near ptr') or t.startswith('ptr'):
            return 4
        if t in SIZES:
            return SIZES[t]
        if t in self.structs:
            return self.structs[t][0]
        if t in self.syms:
            return self.syms[t]
        raise AsmError('unknown type: ' + t)

    def field(self, struct, fld):
        st = self.structs.get(struct.lower())
        if not st or fld.lower() not in st[1]:
            raise AsmError(f'unknown field {struct}.{fld}')
        return st[1][fld.lower()]

    def number_literals(self, s):
        return re.sub(r'(?<![\w.?@$])[0-9][0-9a-f]*[hboqd]?\b',
                      lambda m: str(num(m.group(0))) if num(m.group(0)) is not None else m.group(0), s)

    def var(self, name):
        """(text, size) for an ARG/LOCAL of the current PROC, or None."""
        if self.proc and name in self.proc['vars']:
            return self.proc['vars'][name]
        return None

    def mem(self, inner, size_hint):
        """Translate the inside of [...] to (text, size)."""
        s = inner.strip()
        size = None
        m = re.match(r'(byte|word|dword|fword|qword|tbyte)(\s+ptr)?\s+(.*)$', s)
        if m:
            size = SIZES[m.group(1)]; s = m.group(3)
        seg = ''
        m = re.match(r'([cdefgs]s)\s*:\s*(.*)$', s)
        if m:
            seg = '' if m.group(1) in SEGREGS else m.group(1) + ':'; s = m.group(2)
        # (Struct reg).field  and  (Struct ptr reg).field
        def sref(m):
            st, base, fld = m.group(1), m.group(2), m.group(3)
            off, fs = self.field(st, fld)
            sref.size = fs
            return f'{base}+{off}'
        sref.size = None
        s = re.sub(r'\(\s*(\w+)\s+(?:ptr\s+)?([^()]+?)\s*\)\s*\.\s*(\w+)', sref, s)
        # Struct.field as a plain offset
        s = re.sub(r'\b(\w+)\.(\w+)\b', lambda m: str(self.field(m.group(1), m.group(2))[0]) if m.group(1) in self.structs else m.group(0), s)
        s = re.sub(r'\bsize\s+(\w+)', lambda m: str(self.size_of(m.group(1))), s)
        vsize = None
        def vref(m):
            nonlocal vsize
            w = m.group(0)
            v = self.var(w)
            if v:
                vsize = vsize or v[1]
                return v[0]
            if w in self.syms:
                vsize = vsize or self.syms[w]
            return w
        s = re.sub(r'(?<![\w.?@$])[a-z_?@$][\w?@$]*', vref, s)
        s = self.number_literals(s)
        s = s.replace(' ', '')
        def symexpr(m):
            self.tmp = getattr(self, 'tmp', 0) + 1
            nm = f'.Lexpr{self.tmp}'
            self.out.append(f'.set {nm}, {m.group(0)}')
            return nm
        ident = r'(?<![\w.?@$])(?:(?!(?:' + '|'.join(sorted(REGS, key=len, reverse=True)) + r')\b)[a-z_.?@$][\w.?@$]*)'
        s = re.sub(ident + r'(?:[-+]' + ident + r')+', symexpr, s)
        size = size or sref.size or vsize
        return f'[{seg}{s}]' if not seg else f'{seg}[{s}]', size

    def operand(self, op, is_branch):
        op = op.strip()
        if not op:
            return op, None
        # explicit "dword ptr" outside brackets (MASM style) or IDEAL [dword x]
        m = re.match(r'(byte|word|dword|fword|qword|tbyte)\s+ptr\s+(.*)$', op)
        if m and '[' in m.group(2):
            text, _ = self.mem(m.group(2).strip()[1:-1] if m.group(2).strip().startswith('[') else m.group(2), None)
            return f'{m.group(1)} ptr {text}', SIZES[m.group(1)]
        if op.startswith('[') or re.match(r'[cdefgs]s\s*:', op) or re.match(r'\w+\s*\[', op):
            m2 = re.match(r'([cdefgs]s)\s*:\s*\[(.*)\]$', op)
            if m2:
                return self.mem(m2.group(1) + ':' + m2.group(2), None)
            m3 = re.match(r'(\w+)\s*\[(.*)\]$', op)        # name[reg] -> [name+reg]
            if m3 and m3.group(1) not in REGS:
                return self.mem(m3.group(1) + '+' + m3.group(2), None)
            if not op.endswith(']'):
                raise AsmError('cannot parse operand: ' + op)
            return self.mem(op[1:-1], None)
        if op in REGS:
            return op, None
        if is_branch:
            op = re.sub(r'^(short|near|far)\s+(ptr\s+)?', '', op)
            if op in REGS or op.startswith('['):
                return self.operand(op, False)
            return op, None
        # immediates and addresses
        o = re.sub(r'\boffset\s+', '', op)
        o = re.sub(r'\bsize\s+(\w+)', lambda m: str(self.size_of(m.group(1))), o)
        o = re.sub(r'\b(\w+)\.(\w+)\b', lambda m: str(self.field(m.group(1), m.group(2))[0]) if m.group(1) in self.structs else m.group(0), o)
        o = self.number_literals(o)
        o = re.sub(r'\bshl\b', '<<', o); o = re.sub(r'\bshr\b', '>>', o)
        o = re.sub(r'\bnot\b', '~', o); o = re.sub(r'\band\b', '&', o); o = re.sub(r'\bor\b', '|', o)
        if re.fullmatch(r"'.'", o):
            return str(ord(o[1])), None
        if re.search(r'(?<![\w.?@$])[a-z_?@$][\w?@$]*', o):
            if self.var(o):
                raise AsmError('bare ARG/LOCAL used as a value: ' + op)
            return 'offset ' + o, None
        return o, None

    # ------------------------------------------------------------------ procs
    def label(self, name):
        name = name.lower()
        if name.startswith('??') or name.startswith('@@'):
            if not self.proc:
                return '.Lfile_' + name[2:]
            return f".L{self.proc['name']}_{name[2:]}"
        return name

    def labels_in(self, s):
        return re.sub(r'(\?\?|@@)[\w?@$]+', lambda m: self.label(m.group(0)), s)

    def emit(self, s):
        self.out.append(s)

    def prologue(self):
        p = self.proc
        if p['started']:
            return
        p['started'] = True
        if p['frame']:
            self.emit('    push ebp'); self.emit('    mov ebp, esp')
            if p['local_size']:
                self.emit(f"    sub esp, {p['local_size']}")
        for r in p['uses']:
            self.emit(f'    push {r}')

    def epilogue(self):
        p = self.proc
        for r in reversed(p['uses']):
            self.emit(f'    pop {r}')
        if p['frame']:
            self.emit('    mov esp, ebp'); self.emit('    pop ebp')
        self.emit('    ret')

    def declare_vars(self, kind, rest):
        p = self.proc
        rest = re.sub(r'\s*=\s*\w+\s*$', '', rest)   # ARG ... = argsize
        for item in [x.strip() for x in rest.split(',') if x.strip()]:
            parts = [x.strip() for x in item.split(':')]
            name = parts[0]
            typ = parts[1] if len(parts) > 1 else 'word'
            count = self.eval(parts[2]) if len(parts) > 2 else 1
            esize = self.size_of(typ)
            total = esize * count
            if kind == 'arg':
                p['vars'][name] = (f"ebp+{p['arg_off']}", esize)
                p['arg_off'] += (total + 3) & ~3
            else:
                p['local_size'] += total
                p['vars'][name] = (f"ebp-{p['local_size']}", esize)
            p['frame'] = True
        if kind == 'local':
            p['local_size'] = (p['local_size'] + 3) & ~3

    # ------------------------------------------------------------------- data
    def data_items(self, kind, rest):
        esize = {'db': 1, 'dw': 2, 'dd': 4, 'dq': 8, 'df': 6, 'dt': 10}[kind]
        directive = {1: '.byte', 2: '.short', 4: '.long', 8: '.quad'}[esize] if esize in (1, 2, 4, 8) else None
        items, buf, q, depth = [], '', None, 0
        for ch in rest:
            if q:
                buf += ch
                if ch == q: q = None
            elif ch in '\'"':
                q = ch; buf += ch
            elif ch == '(':
                depth += 1; buf += ch
            elif ch == ')':
                depth -= 1; buf += ch
            elif ch == ',' and depth == 0:
                items.append(buf.strip()); buf = ''
            else:
                buf += ch
        if buf.strip():
            items.append(buf.strip())
        for it in items:
            m = re.match(r'(.+?)\s+dup\s*\((.*)\)$', it, re.I)
            if m:
                n = self.eval(m.group(1)); v = m.group(2).strip()
                if v == '?' or v == '':
                    self.emit(f'    .fill {n}, {esize}, 0')
                else:
                    vals = [self.data_value(x.strip()) for x in v.split(',')]
                    for _ in range(n):
                        self.emit(f"    {directive} {', '.join(vals)}")
            elif it[0] in '\'"' and it[-1] == it[0]:
                if esize != 1:
                    raise AsmError('string in non-byte data')
                self.emit('    .byte ' + ', '.join(str(ord(c)) for c in it[1:-1]))
            elif it == '?':
                self.emit(f'    .fill 1, {esize}, 0')
            else:
                self.emit(f'    {directive} {self.data_value(it)}')

    def data_value(self, v):
        v = re.sub(r'(?i)^offset\s+', '', v.strip()).lower()        # symbols are lowercase everywhere
        try:
            return str(self.eval(v))
        except AsmError:
            return self.labels_in(self.number_literals(v))

    # ------------------------------------------------------------- statement
    def statement(self, ln):
        ln = ln.strip()
        low = ln.lower()
        toks = low.split()
        if not toks:
            return
        w0 = toks[0]
        w1 = toks[1] if len(toks) > 1 else ''

        # inside a STRUC: collect fields only
        if self.cur_struct is not None:
            name, fields, off = self.cur_struct
            if w0 == 'ends' or w0 == 'ends' + '' or (w1 == 'ends'):
                self.structs[name] = (off, fields); self.cur_struct = None
                return
            m = re.match(r'([\w?@$]+)\s+(db|dw|dd|dq|df|dt)\s+(.*)$', low)
            if m:
                esize = {'db': 1, 'dw': 2, 'dd': 4, 'dq': 8, 'df': 6, 'dt': 10}[m.group(2)]
                cnt = 0
                for it in m.group(3).split(','):
                    d = re.match(r'(.+?)\s+dup\s*\(', it.strip())
                    cnt += self.eval(d.group(1)) if d else 1
                fields[m.group(1)] = (off, esize)
                self.cur_struct = (name, fields, off + esize * cnt)
                return
            m = re.match(r'([\w?@$]+)\s+(\w+)\s*(<.*>|\?)?\s*$', low)
            if m and m.group(2) in self.structs:
                sz = self.structs[m.group(2)][0]
                fields[m.group(1)] = (off, sz)
                self.cur_struct = (name, fields, off + sz)
                return
            raise AsmError('unrecognised STRUC member: ' + ln)

        if w0 == 'struc' or w1 == 'struc':
            name = w1 if w0 == 'struc' else w0
            self.cur_struct = (name, {}, 0)
            return
        if w0 in ('codeseg', '.code'):
            self.emit('.text'); self.section = 'text'; return
        if w0 in ('dataseg', '.data', 'udataseg', 'const'):
            self.emit('.data'); self.section = 'data'; return
        if w1 == 'segment' or w0 == 'segment':                 # NAME SEGMENT ... / SEGMENT NAME ...
            self.emit('.text' if "'code'" in low or 'code' in low.split() else '.data'); return
        if w0 in IGNORE or w1 == 'ends':
            return
        if w0 == 'global':
            m = re.match(r'global\s+(?:c\s+|pascal\s+|syscall\s+)?([\w?@$]+)\s*:\s*(\w+)', low)
            if not m:
                raise AsmError('bad GLOBAL: ' + ln)
            nm, typ = m.group(1), m.group(2)
            self.globals.add(nm)
            if typ in ('byte', 'word', 'dword', 'qword', 'fword') and nm not in self.syms:
                self.syms[nm] = SIZES[typ]
            return
        if w0 in ('align', 'even'):
            self.emit(f"    .balign {self.eval(toks[1]) if w0 == 'align' else 2}"); return

        # equates
        m = re.match(r'([\w?@$]+)\s*(=)\s*(.*)$', low) or re.match(r'([\w?@$]+)\s+(equ)\s+(.*)$', low)
        if m:
            val = m.group(3).strip()
            if val.startswith('<') and val.endswith('>'):
                val = val[1:-1]
            try:
                val = str(self.eval(val))
            except Exception:
                pass
            self.equ[m.group(1)] = val
            return

        # procedures
        m = re.match(r'proc\s+([\w?@$]+)(.*)$', low) or (re.match(r'([\w?@$]+)\s+proc\b(.*)$', low))
        if m:
            name = m.group(1)
            self.proc = {'name': name, 'vars': {}, 'uses': [], 'arg_off': 8, 'local_size': 0,
                         'frame': False, 'started': False}
            self.emit(f'.globl {name}'); self.emit(f'{name}:')
            self.code_labels.add(name)
            mu = re.search(r'\buses\s+(.*)$', m.group(2))
            if mu:
                self.proc['uses'] = [r.strip() for r in re.split(r'[,\s]+', mu.group(1)) if r.strip()]
            return
        if w0 == 'endp' or w1 == 'endp':
            self.proc = None; return
        if self.proc and w0 == 'uses':
            self.proc['uses'] += [r.strip() for r in re.split(r'[,\s]+', low[4:]) if r.strip()]
            return
        if self.proc and w0 in ('arg', 'local'):
            if self.proc['started']:
                raise AsmError(f'{w0.upper()} after the first instruction in {self.proc["name"]}')
            self.declare_vars(w0, low[len(w0):].strip())
            return

        # labels
        m = re.match(r'([\w?@$]+)\s*:(?!:)\s*(.*)$', low)
        if m and m.group(1) not in REGS:
            if self.proc:
                self.prologue()
            lab = self.label(m.group(1))
            self.emit(f'{lab}:')
            if not lab.startswith('.L'):
                self.code_labels.add(lab)
            if m.group(2):
                self.statement(ln[ln.index(':') + 1:].strip())
            return
        m = re.match(r'label\s+([\w?@$]+)\s+(\w+)', low) or re.match(r'([\w?@$]+)\s+label\s+(\w+)', low)
        if m:
            nm = m.group(1)
            if self.proc: self.prologue()
            self.emit(f'{self.label(nm)}:')
            if m.group(2) in SIZES and m.group(2) not in ('near', 'far', 'proc'):
                self.syms[nm] = SIZES[m.group(2)]
            else:
                self.code_labels.add(nm)
            return

        # data definitions
        m = re.match(r'(?:([\w?@$]+)\s+)?(db|dw|dd|dq|df|dt)\s+(.*)$', low, re.I)
        if m:
            if m.group(1):
                nm = self.label(m.group(1))
                self.syms[nm] = {'db': 1, 'dw': 2, 'dd': 4, 'dq': 8, 'df': 6, 'dt': 10}[m.group(2)]
                self.emit(f'{nm}:')
            # keep string case: re-slice original line
            orig = ln[ln.lower().index(m.group(2) + ' ') + 3:] if (m.group(2) + ' ') in ln.lower() else m.group(3)
            self.data_items(m.group(2), orig.strip())
            return
        m = re.match(r'([\w?@$]+)\s+(\w+)\s*(<.*>|\?)\s*$', low)
        if m and m.group(2) in self.structs:
            self.syms[m.group(1)] = 1
            self.emit(f'{m.group(1)}:'); self.emit(f'    .fill {self.structs[m.group(2)][0]}, 1, 0')
            return

        # instructions
        if self.proc:
            self.prologue()
        self.instruction(ln)

    def instruction(self, ln):
        low = self.subst_equ(ln.lower())
        low = self.labels_in(low)
        if re.search(r'(?<![\w?@$])\$(?![\w?@$])', low):            # $ = this instruction's address
            self.tmp = getattr(self, 'tmp', 0) + 1
            here = f'.Lhere{self.tmp}'
            self.emit(f'{here}:')
            low = re.sub(r'(?<![\w?@$])\$(?![\w?@$])', here, low)
        m = re.match(r'((?:rep|repe|repz|repne|repnz|lock)\s+)?([a-z][a-z0-9]*)\s*(.*)$', low)
        if not m:
            raise AsmError('cannot parse: ' + ln)
        prefix, op, rest = (m.group(1) or ''), m.group(2), m.group(3)
        if op == 'ret' and self.proc and (self.proc['frame'] or self.proc['uses']):
            if rest.strip():
                raise AsmError('RET n inside a framed PROC')
            self.epilogue(); return
        mc = re.match(r'call\s+([\w?@$]+)\s+(c|pascal|stdcall|syscall)\s*,\s*(.*)$', low)
        if mc:
            args = [a.strip() for a in mc.group(3).split(',')]
            if mc.group(2) != 'c':
                raise AsmError('only C-language extended CALL is supported: ' + ln)
            for a in reversed(args):
                self.instruction('push ' + a)
            self.emit(f'    call {mc.group(1)}')
            self.emit(f'    add esp, {4 * len(args)}')
            return
        if op == 'retn':
            op = 'ret'
        if op == 'xlat' or op == 'xlatb':
            self.emit('    xlatb'); return
        if op in ('lds', 'les', 'lfs', 'lgs', 'lss'):
            raise AsmError(f'{op.upper()} (far pointer load) is not supported: ' + ln)
        if op in ('pushad', 'popad', 'pushfd', 'popfd', 'cld', 'std', 'cdq', 'cwd', 'cbw', 'cwde', 'stosb',
                  'stosw', 'stosd', 'movsb', 'movsw', 'movsd', 'lodsb', 'lodsw', 'lodsd', 'scasb', 'scasw',
                  'scasd', 'cmpsb', 'cmpsw', 'cmpsd', 'clc', 'stc', 'cmc', 'nop', 'lahf', 'sahf', 'pushf',
                  'popf', 'pusha', 'popa', 'ret', 'leave', 'cli', 'sti', 'cpuid', 'rdtsc', 'emms', 'int3') and not rest:
            self.emit(f'    {prefix}{op}'); return
        # split operands at top-level commas
        ops, buf, depth, q = [], '', 0, None
        for ch in rest:
            if q:
                buf += ch
                if ch == q: q = None
                continue
            if ch in '\'"': q = ch
            if ch in '[(': depth += 1
            if ch in '])': depth -= 1
            if ch == ',' and depth == 0:
                ops.append(buf); buf = ''
            else:
                buf += ch
        if buf.strip():
            ops.append(buf)
        is_branch = op.startswith('j') or op in ('call', 'loop', 'loope', 'loopne', 'loopz', 'loopnz')
        tr = [self.operand(o, is_branch) for o in ops]
        texts = []
        for i, (t, size) in enumerate(tr):
            if t.startswith('[') or re.match(r'[fg]s:\[', t):
                other_reg = any(o.strip() in REGS for j, o in enumerate(ops) if j != i)
                needs = size and (not other_reg or op in ('movzx', 'movsx', 'call', 'jmp', 'push', 'pop')
                                  or op.startswith('sh') or op.startswith('ro') or op.startswith('rc') or op == 'sar')
                if op == 'lea':
                    needs = False
                if needs:
                    t = f'{PTR[size]} ptr {t}'
            texts.append(t)
        if op == 'push' and len(texts) == 1 and texts[0].startswith('offset '):
            # TASM pushed a 32-bit address; LLVM picks a 16-bit push for
            # `push offset sym` in Intel syntax. Emit push imm32 explicitly.
            self.emit('    .byte 0x68')
            self.emit(f'    .long {texts[0][7:]}')
            return
        self.emit(f"    {prefix}{op} {', '.join(texts)}".rstrip())

    # ----------------------------------------------------------- macros, ifs
    def expand(self, lines):
        """Conditional assembly, MACRO and REPT expansion, as a line stream."""
        stack = []          # [(active, taken)]
        it = iter(lines)
        for ln in it:
            low = ln.lower()
            toks = low.split()
            w0 = toks[0] if toks else ''
            w1 = toks[1] if len(toks) > 1 else ''
            active = all(a for a, _ in stack)
            ml = re.match(r'([\w?@$]+)\s*:(?!:)\s*(\S.*)$', ln.strip())
            if active and ml and ml.group(2).split()[0].lower() in self.macros:
                yield ml.group(1) + ':'                  # "label: MACRO args" -> two lines
                yield from self.expand([ml.group(2)])
                continue
            if w0 in ('if', 'ifdef', 'ifndef', 'ife'):
                if not active:
                    stack.append((False, True)); continue
                arg = low[len(w0):].strip()
                if w0 == 'if':      c = bool(self.eval(arg))
                elif w0 == 'ife':   c = not self.eval(arg)
                elif w0 == 'ifdef': c = arg in self.equ
                else:               c = arg not in self.equ
                stack.append((c, c)); continue
            if w0 == 'else':
                a, taken = stack.pop()
                parent = all(x for x, _ in stack)
                stack.append((parent and not taken, True)); continue
            if w0 == 'endif':
                stack.pop(); continue
            if not active:
                continue
            if w1 == 'macro' or w0 == 'macro':
                name = w0 if w1 == 'macro' else w1
                params = [p.strip() for p in low.split('macro', 1)[1].replace(name, '', 1 if w0 == 'macro' else 0).split(',') if p.strip()]
                body, depth = [], 1
                for b in it:
                    bl = b.lower().split()
                    if bl and (bl[0] in ('rept', 'irp', 'irpc') or (len(bl) > 1 and bl[1] == 'macro')):
                        depth += 1
                    if bl and bl[0] == 'endm':
                        depth -= 1
                        if depth == 0:
                            break
                    body.append(b)
                self.macros[name] = (params, body)
                continue
            if w0 == 'rept':
                n = self.eval(low[4:].strip())
                body, depth = [], 1
                for b in it:
                    bl = b.lower().split()
                    if bl and (bl[0] in ('rept', 'irp', 'irpc') or (len(bl) > 1 and bl[1] == 'macro')):
                        depth += 1
                    if bl and bl[0] == 'endm':
                        depth -= 1
                        if depth == 0:
                            break
                    body.append(b)
                for _ in range(n):
                    self.macro_count = getattr(self, 'macro_count', 0) + 1
                    yield from self.expand(self.localise(body))
                continue
            if w0 in self.macros:
                params, body = self.macros[w0]
                args = [a.strip() for a in ln[len(w0):].split(',')] if ln[len(w0):].strip() else []
                self.macro_count = getattr(self, 'macro_count', 0) + 1
                sub = {}
                for i, p in enumerate(params):
                    sub[p] = args[i] if i < len(args) else ''
                out = []
                local_labels = {}
                for b in body:
                    bl = b.lower().split()
                    if bl and bl[0] == 'local':
                        for l in b[5:].split(','):
                            local_labels[l.strip().lower()] = f'??m{self.macro_count}_{l.strip().lower().lstrip("?@")}'
                        continue
                    t = b
                    for k, v in list(sub.items()) + list(local_labels.items()):
                        t = re.sub(r'(?i)(?<![\w?@$])&?' + re.escape(k) + r'&?(?![\w?@$])', v, t)
                    out.append(t)
                yield from self.expand(out)
                continue
            yield ln

    def localise(self, body):
        """Give a REPT/MACRO body's LOCAL labels fresh names for this expansion."""
        names, out = {}, []
        for b in body:
            bl = b.lower().split()
            if bl and bl[0] == 'local':
                for l in b[5:].split(','):
                    names[l.strip().lower()] = f'??m{self.macro_count}_{l.strip().lower().lstrip("?@")}'
                continue
            out.append(b)
        if names:
            out = [re.sub(r'(?i)(?<![\w?@$])(' + '|'.join(re.escape(k) for k in names) + r')(?![\w?@$])',
                          lambda m: names[m.group(1).lower()], b) for b in out]
        return out

    def convert(self, path):
        self.emit('.intel_syntax noprefix')
        self.emit('.text')
        lines = list(self.lines_of(path))
        for ln in self.expand(lines):
            try:
                self.statement(ln)
            except AsmError as e:
                raise AsmError(f'{os.path.basename(path)}: {e}\n    line: {ln}')
        self.out += [f'.globl {g}' for g in sorted(self.globals)]   # TASM GLOBAL: export or import
        return '\n'.join(self.out) + '\n'


def makefile_defines(asm_path):
    """The /dNAME=VALUE options the file's own MAKEFILE passed to TASM."""
    d = os.path.dirname(os.path.abspath(asm_path))
    mk = ci_path(d, 'MAKEFILE')
    out = {}
    if mk:
        for m in re.finditer(r'(?i)ASM_OPTS\s*=\s*(.*)', open(mk, encoding='latin-1').read()):
            for k, v in re.findall(r'/d(\w+)(?:=(\S+))?', m.group(1), re.I):
                out[k] = v or '1'
    return out


def main():
    args = sys.argv[1:]
    out = None; defines = {}
    if '-o' in args:
        i = args.index('-o'); out = args[i + 1]; del args[i:i + 2]
    while '-D' in args:
        i = args.index('-D'); k, _, v = args[i + 1].partition('='); defines[k] = v or '1'; del args[i:i + 2]
    defines = {**makefile_defines(args[0]), **defines}
    try:
        text = Converter(defines).convert(args[0])
    except AsmError as e:
        sys.exit(f'tasm2gas: {e}')
    if out:
        open(out, 'w').write(text)
    else:
        sys.stdout.write(text)


if __name__ == '__main__':
    main()
