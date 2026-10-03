#!/usr/bin/env python3
"""
worklist.py -- regenerate port/WORKLIST.md, port/worklist.json and
port/asm-inventory.json from the current state of the tree.

Run after port/probe.sh (it reads probe's results). Everything is derived from
the source and the compiler, except the explicit, reasoned tables below for the
cases static analysis cannot decide.

How liveness is decided, and why
--------------------------------
Neither CODE/MAKEFILE nor the IDE project RA95.PJT is a complete record of what
the shipped game linked: UNIT.CPP calls Fixed_To_Cardinal, defined only in
COORDA.ASM, which the makefile never names; and RA95.PJT lists BOTH halves of
every DOS/Win32 pair. So:

  * C++ translation units: port/build-set.txt (see gen-build-set.py).
  * Assembly: a file is LIVE if live code calls something it exports, computed
    as a closure (assembly can call assembly). If every called export already
    has a live C/C++ definition, the file is SUPERSEDED -- Westwood translated
    several routines themselves and left the assembly in place.
"""
import glob, json, os, re, sys, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
PROBE = os.path.join(os.environ.get('TMPDIR', '/tmp'), 'ra-probe')

# --------------------------------------------------------------------------
# Out of scope for the single-player build. Explicit, because filename
# patterns got it wrong both ways (EGOS.CPP is the credits roll, not
# multiplayer; DIBUTIL.CPP looks like graphics but only serves Westwood Online).
# --------------------------------------------------------------------------
DROP = {}
for f in ('CONNECT COMBUF COMQUEUE PACKET QUEUE SESSION NETDLG MPLAYER MPGSET '
          'IPX IPX95 IPXADDR IPXCONN IPXGCONN IPXMGR NULLCONN NULLDLG NULLMGR '
          'TCPIP INTERNET WSPROTO WSPIPX WSPUDP _WSPROTO BIGCHECK DDE CCDDE '
          'CCMPATH CCTEN TENMGR MPMGRD MPMGRW MODEMREG SENDFILE STATS').split():
    DROP[f] = 'multiplayer / online / serial'
for f in glob.glob('CODE/WOL*.CPP'):
    DROP[os.path.basename(f)[:-4].upper()] = 'Westwood Online (service defunct)'
for f, why in (('DIBUTIL', 'Win32 GDI bitmaps; only callers are WOLAPIOB.CPP and ICONLIST.CPP'),
               ('DIBFILE', 'Win32 GDI bitmap files; only caller is DIBUTIL'),
               ('ICONLIST', 'IconListClass; used only by WOL_* and TOOLTIP'),
               ('TOOLTIP', 'ToolTipClass; used only by WOL_* and ICONLIST'),
               ('WOLAPIOB', 'Westwood Online API objects'),
               ('WOLSTRNG', 'Westwood Online strings')):
    DROP[f] = why

# --------------------------------------------------------------------------
# Assembly the reference closure cannot classify correctly on its own.
# --------------------------------------------------------------------------
ASM_OVERRIDE = {
    'WIN32LIB/DRAWBUFF/SHADOW.ASM':  ('DEAD', 'Shadow_Blit is only called in the DOS #else branch of GSCREEN.CPP; Hide/Show_Mouse have C versions in MOUSE.CPP'),
    'WIN32LIB/PLAYCD/PLAYCD.ASM':    ('NATIVE', 'DOS DPMI real-mode CD access; Redbook music becomes AVFoundation'),
    'WIN32LIB/KEYBOARD/WWMOUSE.ASM': ('NATIVE', 'software cursor; NSCursor or composite in the Metal shader'),
    'CODE/CPUID.ASM':                ('REBUILD', 'x86 CPUID; no arm64 equivalent'),
    'WIN32LIB/MISC/OPSYS.ASM':       ('REBUILD', 'x86/DOS OS detection'),
    'WIN32LIB/MISC/DETPROC.ASM':     ('REBUILD', 'x86 processor detection'),
    'WINVQ/VQM32/SOSCODEC.ASM':      ('SUPERSEDED', 'VQA_-prefixed copy of the SOS ADPCM codec; wrap CODE/ADPCM.CPP'),
    'WIN32LIB/AUDIO/OLSOSDEC.ASM':   ('TRANSLATE', 'General_sosCODECDecompressData is live (SOUNDINT.CPP): adapt CODE/ADPCM.CPP rather than translate from scratch'),
}
TOOLS = re.compile(r'WINVQ/(VQAVIEW|VPLAY32)/')

strip = lambda t: re.sub(r'//[^\n]*|/\*.*?\*/', '', t, flags=re.S)
read = lambda f: open(f, encoding='latin-1').read()

build = [l.strip() for l in open('port/build-set.txt') if l.strip()]
live_c = ['CODE/' + b for b in build] + [
    f for d in ('WIN32LIB', 'WINVQ') for f in glob.glob(d + '/**/*', recursive=True)
    if re.search(r'\.(CPP|cpp|C|c)$', f) and not TOOLS.search(f)]
ctext = {f: strip(read(f)) for f in live_c}
# Headers matter: most of the renderer is reached through inline wrappers in
# WIN32LIB/INCLUDE (e.g. GBUFFER.H calling ::Buffer_Fill_Rect(this, ...)).
headers = [f for d in ('CODE', 'WIN32LIB', 'WINVQ') for f in glob.glob(d + '/**/*', recursive=True)
           if re.search(r'\.(H|h)$', f) and not TOOLS.search(f)]
htext = {f: strip(read(f)) for f in headers}

def calls_in(t):
    """Free-function calls, including `::Name(` (explicit global scope), but not
    `obj.Name(`, `ptr->Name(` or `Class::Name(`."""
    out = set()
    for m in re.finditer(r'(\w+)\s*\(', t):
        pre = t[max(0, m.start() - 2):m.start()]
        if pre.endswith('.') or pre.endswith('->') or pre.endswith('>'):
            continue
        if pre == '::':
            if re.search(r'\w::$', t[max(0, m.start() - 3):m.start()]):
                continue                     # Class::Name(  -- a member
        out.add(m.group(1))
    return out

called = set()
for t in list(ctext.values()) + list(htext.values()):
    called |= calls_in(t)
cdefs = set()
for t in ctext.values():
    cdefs |= set(m.group(1) for m in re.finditer(
        r'^[A-Za-z_][\w\s\*&]*?(?<![:\w])(\w+)\s*\([^;{]*\)\s*\{', t, re.M))

asms = sorted(f for f in glob.glob('CODE/*.ASM') + [
    x for d in ('WIN32LIB', 'WINVQ') for x in glob.glob(d + '/**/*.ASM', recursive=True)]
    if not TOOLS.search(f))

def exports(src):
    e = set(m.group(1) for m in re.finditer(r'^\s*(?:GLOBAL|global|PUBLIC|public)\s+(?:C\s+)?([A-Za-z_]\w*)', src, re.M))
    e |= set(m.group(1) for m in re.finditer(r'^\s*(?:PROC|proc)\s+(?:C\s+)?([A-Za-z_]\w*)', src, re.M))
    return {x.strip('_') for x in e} - {'C', 'NOLANGUAGE'}

asm_src = {a: read(a) for a in asms}
asm_exp = {a: exports(asm_src[a]) for a in asms}

# Closure: an assembly file is live if live C, or live assembly, calls an export.
live = {a for a in asms if asm_exp[a] & called}
while True:
    words = set()
    for a in live:
        words |= set(re.findall(r'\w+', asm_src[a]))
    more = {a for a in asms if a not in live and asm_exp[a] & words}
    if not more:
        break
    live |= more

asm_rows = []
for a in asms:
    lines = asm_src[a].count('\n')
    used = sorted(asm_exp[a] & called)
    if a in ASM_OVERRIDE:
        tag, why = ASM_OVERRIDE[a]
    elif a not in live:
        tag, why = 'DEAD', 'no live caller'
    elif used and all(u in cdefs for u in used):
        tag, why = 'SUPERSEDED', 'C already defines: ' + ', '.join(used)
    else:
        tag, why = 'TRANSLATE', ('no C yet for: ' + ', '.join(u for u in used if u not in cdefs)) if used else 'called from other assembly'
    asm_rows.append({'file': a, 'lines': lines, 'tag': tag, 'note': why})

# --------------------------------------------------------------------------
# C++ translation units, from the last probe run.
# --------------------------------------------------------------------------
status = {}
res = os.path.join(PROBE, 'results.txt')
if not os.path.exists(res):
    sys.exit('run port/probe.sh first')
for line in open(res):
    p = line.split()
    if len(p) == 2:
        status[p[1]] = p[0]

def first_error(f):
    lg = os.path.join(PROBE, f + '.log')
    if os.path.exists(lg):
        for ln in open(lg, errors='replace'):
            if 'error:' in ln:
                return re.sub(r'^.*?(fatal )?error: ', '', ln).strip()[:110]
    return ''

code_rows = []
for f in build:
    stem = f[:-4].upper()
    if status.get(f) == 'OK':
        tag, note = 'DONE', 'compiles clean for arm64'
    elif stem in DROP:
        tag, note = 'DROP', DROP[stem]
    else:
        tag, note = 'TWEAK', first_error(f)
    code_rows.append({'file': 'CODE/' + f, 'tag': tag, 'note': note})

json.dump({'code': code_rows, 'asm': asm_rows}, open('port/worklist.json', 'w'), indent=1)
json.dump(asm_rows, open('port/asm-inventory.json', 'w'), indent=1)

# --------------------------------------------------------------------------
# WORKLIST.md
# --------------------------------------------------------------------------
cc = collections.Counter(r['tag'] for r in code_rows)
ac = collections.Counter(r['tag'] for r in asm_rows)
al = collections.Counter()
for r in asm_rows:
    al[r['tag']] += r['lines']

def sig(n):
    return re.sub(r"'[^']*'", "'X'", n or '')[:72] or '(no error captured)'

groups = collections.defaultdict(list)
for r in code_rows:
    if r['tag'] == 'TWEAK':
        groups[sig(r['note'])].append(os.path.basename(r['file']))

out = ['# Worklist -- what needs doing, per file', '',
       'Generated by `port/worklist.py` from a live `port/probe.sh` run. Do not edit by',
       'hand; rerun the two scripts. Machine-readable twin: `port/worklist.json`.', '',
       '## C++ translation units (`port/build-set.txt`)', '',
       '| Tag | Meaning | Files |', '|---|---|---|',
       f"| DONE | compiles clean for arm64 | {cc['DONE']} |",
       f"| TWEAK | needs source or compat fixes | {cc['TWEAK']} |",
       f"| DROP | out of scope for single-player; left in place because live code includes its headers | {cc['DROP']} |",
       '', '## Assembly', '',
       '| Tag | Meaning | Files | Lines |', '|---|---|---|---|']
for t, m in (('TRANSLATE', 'live, no C yet: rewrite as portable C'),
             ('SUPERSEDED', 'live, but a C/C++ definition already exists in the tree'),
             ('NATIVE', 'needed, reimplemented on a macOS framework'),
             ('REBUILD', 'no arm64 equivalent; new logic'),
             ('DEAD', 'nothing live calls it')):
    out.append(f"| {t} | {m} | {ac[t]} | {al[t]:,} |")
out += ['', '---', '', f"## TWEAK -- {cc['TWEAK']} files, grouped by first error", '']
for s, fs in sorted(groups.items(), key=lambda kv: -len(kv[1])):
    out += [f"### {s} ({len(fs)})", '', ', '.join(f'`{x}`' for x in sorted(fs)), '']
for t in ('TRANSLATE', 'SUPERSEDED', 'NATIVE', 'REBUILD', 'DEAD'):
    rows = sorted((r for r in asm_rows if r['tag'] == t), key=lambda r: -r['lines'])
    out += ['---', '', f"## {t} -- {len(rows)} files, {al[t]:,} lines", '',
            '| File | Lines | Note |', '|---|---|---|']
    out += [f"| `{r['file']}` | {r['lines']:,} | {r['note']} |" for r in rows]
    out.append('')
out += ['---', '', f"## DROP -- {cc['DROP']} files", '', '| File | Reason |', '|---|---|']
out += [f"| `{os.path.basename(r['file'])}` | {r['note']} |" for r in code_rows if r['tag'] == 'DROP']
open('port/WORKLIST.md', 'w').write('\n'.join(out) + '\n')

print(f"C++: {dict(cc)}", file=sys.stderr)
print(f"asm: " + ', '.join(f"{t} {ac[t]} files/{al[t]:,} lines" for t in ac), file=sys.stderr)
