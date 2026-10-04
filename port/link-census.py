#!/usr/bin/env python3
"""
link-census.py <linkdir> -- classify the undefined and duplicate symbols from a
link-census.sh run, and write port/LINK-CENSUS.md.

Each undefined symbol gets the first matching cause:

  TEMPLATE      a template member defined in a .CPP but never instantiated
  PRAGMA-AUX    a Watcom `#pragma aux` inline-assembly function (no C body)
  ASM:<tag>     exported by a live assembly file (tag from asm-inventory)
  NATIVE-CPP    defined in a game/library file reimplemented over port/backend
  DROP          defined in an out-of-scope (multiplayer, DOS, ...) file
  ARCHIVED      defined only in archive/
  COMPAT        declared by port/compat/*.h, not implemented yet
  ENTRY         the program entry point
  NOWHERE       not defined anywhere in the tree
"""
import collections, glob, json, os, re, sys

LINK = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

link = open(os.path.join(LINK, 'link.txt'), errors='replace').read()
undef = re.findall(r'^\s+"([^"]+)", referenced from:', link, re.M)
dups = re.findall(r'^duplicate symbol \'([^\']+)\'', link, re.M)
compile_fail = []
for log in glob.glob(os.path.join(LINK, '*', '*.o.log')):
    if os.path.getsize(log) and 'error:' in open(log, errors='replace').read():
        compile_fail.append(os.path.basename(log)[:-6])

strip = lambda t: re.sub(r'//[^\n]*|/\*.*?\*/', '', t, flags=re.S)
read = lambda f: open(f, encoding='latin-1').read()
w = json.load(open('port/worklist.json'))

def name_of(sym):
    if '(' not in sym and sym.startswith('_') and '::' not in sym:
        return sym[1:]
    return sym.split('(')[0]

pragma_aux = set()
for f in glob.glob('CODE/*') + glob.glob('WIN32LIB/**/*', recursive=True) + glob.glob('WINVQ/**/*', recursive=True):
    if re.search(r'\.(H|h|CPP|cpp)$', f):
        pragma_aux |= set(re.findall(r'^\s*#\s*pragma\s+aux\s+(\w+)', read(f), re.M))

asm_origin = {}
for r in w['asm']:
    for m in re.finditer(r'^\s*(?:GLOBAL|global|PUBLIC|public|PROC|proc)\s+(?:C\s+)?([A-Za-z_]\w*)', read(r['file']), re.M):
        asm_origin.setdefault(m.group(1).strip('_'), (r['tag'], r['file']))

defpat = re.compile(r'^[A-Za-z_][\w\s\*&<>,:~]*?\b((?:\w+::)*~?\w+)\s*\([^;{]*\)\s*(?:const\s*)?(?::[^{;]*)?\{', re.M)
cpp_origin = {}
for rows, kind in ((w['code'], 'game'), (w['lib'], 'lib')):
    for r in rows:
        if r['tag'] == 'DONE':
            continue
        for m in defpat.finditer(strip(read(r['file']))):
            cpp_origin.setdefault(m.group(1), ('NATIVE-CPP' if r['tag'] == 'NATIVE' else 'DROP', r['file']))

archived = set()
for f in glob.glob('archive/**/*', recursive=True):
    if re.search(r'\.(CPP|cpp|C|c|ASM|asm)$', f):
        t = read(f)
        archived |= set(m.group(1) for m in defpat.finditer(strip(t)))
        archived |= set(x.strip('_') for x in re.findall(r'^\s*(?:GLOBAL|global|PROC|proc)\s+(?:C\s+)?([A-Za-z_]\w*)', t, re.M))

compat = set()
for f in glob.glob('port/compat/*.h'):
    compat |= set(re.findall(r'\b(\w+)\s*\([^;{)]*\)\s*;', strip(open(f, encoding='utf-8').read())))

cats = collections.defaultdict(list)
for sym in undef:
    n = name_of(sym); base = n.split('::')[-1]
    if re.match(r'^\w+<.*>::', n):                         cat = 'TEMPLATE'
    elif base in pragma_aux:                                cat = 'PRAGMA-AUX'
    elif base in asm_origin:                                cat = 'ASM:' + asm_origin[base][0]
    elif n in cpp_origin or base in cpp_origin:            cat = (cpp_origin.get(n) or cpp_origin[base])[0]
    elif n == 'main':                                       cat = 'ENTRY'
    elif base in compat:                                    cat = 'COMPAT'
    elif n in archived or base in archived:                cat = 'ARCHIVED'
    else:                                                   cat = 'NOWHERE'
    cats[cat].append(sym)

order = ['TEMPLATE', 'PRAGMA-AUX', 'ASM:TRANSLATE', 'ASM:SUPERSEDED', 'ASM:NATIVE', 'ASM:REBUILD', 'ASM:DEAD',
         'NATIVE-CPP', 'DROP', 'ARCHIVED', 'COMPAT', 'ENTRY', 'NOWHERE']
meaning = {
    'TEMPLATE': 'template member defined in a .CPP but never instantiated',
    'PRAGMA-AUX': 'Watcom `#pragma aux` inline-assembly function: needs a C body',
    'ASM:TRANSLATE': 'exported by live assembly with no C yet: translate',
    'ASM:SUPERSEDED': 'exported by assembly that a C version replaces: wire the C version in',
    'ASM:NATIVE': 'exported by assembly being replaced natively',
    'ASM:REBUILD': 'exported by x86-specific assembly: new logic',
    'ASM:DEAD': 'exported by assembly believed dead -- something does call it',
    'NATIVE-CPP': 'defined in a platform-layer file being reimplemented over port/backend',
    'DROP': 'defined in an out-of-scope file: the caller needs a stub or a guard',
    'ARCHIVED': 'defined only in archive/',
    'COMPAT': 'declared by port/compat, not implemented yet',
    'ENTRY': 'program entry point',
    'NOWHERE': 'not defined anywhere in the tree',
}
total = len(undef)
print(f"undefined symbols: {total}   duplicate symbols: {len(dups)}   objects that failed to compile: {len(compile_fail)}")
for c in order + sorted(set(cats) - set(order)):
    if cats.get(c):
        print(f"  {len(cats[c]):4d}  {c:15s} {meaning.get(c, '')}")

out = ['# Link census', '',
       'Generated by `port/link-census.sh`. Do not edit by hand.', '',
       'Every translation unit that compiles is linked once; this is every symbol',
       'the link still cannot resolve, by cause. When this list is empty, the game',
       'links.', '',
       f'**Undefined: {total}. Duplicate: {len(dups)}.**', '',
       '| Cause | Symbols | Meaning |', '|---|---|---|']
for c in order + sorted(set(cats) - set(order)):
    if cats.get(c):
        out.append(f"| {c} | {len(cats[c])} | {meaning.get(c, '')} |")
for c in order + sorted(set(cats) - set(order)):
    if cats.get(c):
        out += ['', f"## {c} ({len(cats[c])})", '', '```']
        out += sorted(cats[c])
        out.append('```')
if dups:
    out += ['', f"## Duplicate symbols ({len(dups)})", '', '```'] + sorted(set(dups)) + ['```']
open('port/LINK-CENSUS.md', 'w').write('\n'.join(out) + '\n')
json.dump(cats, open(os.path.join(LINK, 'census.json'), 'w'), indent=1)
