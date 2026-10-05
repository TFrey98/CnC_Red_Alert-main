#!/usr/bin/env python3
"""
gen-build-set.py -- derive the translation units the shipped WIN32 game was
built from, straight from CODE/MAKEFILE, and write port/build-set.txt.

ra95.exe links OBJECTS plus two libraries built from TECHFILES (tech.lib) and
LIBFILES (jshell.lib). All three lists are read, honouring !ifdef WIN32 /
!else / !endif, so DOS-only objects (KEYFRAME, KEYFBUFF, ...) drop out.

The makefile is NOT a complete record of what the shipped game linked: UNIT.CPP
calls Fixed_To_Cardinal, defined only in COORDA.ASM, which no makefile list
names. RA95.PJT (the IDE project) is no better -- it lists BOTH halves of every
DOS/Win32 pair. So the set is the makefile's, plus files live code demonstrably
depends on (it calls something defined only there):

  + PALETTE   PaletteClass, reconstructed by the port (never released)
  + CSTRAW    CacheStraw, used by live code
  + RAND      Sim_IRandom, called from MPLAYER.CPP
  + PRAGMAUX  C bodies for Watcom `#pragma aux` inline-asm functions (port-created)
  + NETSTUB   inert multiplayer stand-ins for the single-player build (port-created)
  + PORTSTUB  stand-ins for DOS/debug-only code the Mac build lacks (port-created)

(CODE/LCWUNCMP.CPP, Westwood's C LCW_Uncompress, was once an extra too. It
ignores its length argument, which the shipped assembly honours; the build now
uses WIN32LIB/IFF/LCWUNCMP.CPP, a verified translation of that assembly.)
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTRA = ['PALETTE', 'CSTRAW', 'RAND', 'PRAGMAUX', 'NETSTUB', 'PORTSTUB']

objs, stack, cur = set(), [], None
for raw in open(os.path.join(ROOT, 'CODE', 'MAKEFILE'), encoding='latin-1'):
    ln = raw.strip()
    if ln.startswith('!ifdef'):  stack.append(ln.split()[1].upper() == 'WIN32'); cur = None; continue
    if ln.startswith('!ifndef'): stack.append(ln.split()[1].upper() != 'WIN32'); cur = None; continue
    if ln.startswith('!else'):   stack[-1] = not stack[-1]; cur = None; continue
    if ln.startswith('!endif'):  stack.pop(); cur = None; continue
    m = re.match(r'(OBJECTS|LIBFILES|TECHFILES)\s*\+?=(.*)', ln)
    if m: cur, ln = m.group(1), m.group(2)
    if cur:
        if all(stack):
            objs.update(o.upper() for o in re.findall(r'([A-Za-z0-9_]+)\.OBJ', ln, re.I))
        if not ln.rstrip().endswith('&'): cur = None

objs.update(EXTRA)
code = {os.path.splitext(f)[0].upper(): f for f in os.listdir(os.path.join(ROOT, 'CODE'))
        if f.upper().endswith('.CPP')}
tus = sorted(code[o] for o in objs if o in code)
with open(os.path.join(ROOT, 'port', 'build-set.txt'), 'w') as fp:
    fp.write('\n'.join(tus) + '\n')
print(f"{len(tus)} translation units -> port/build-set.txt "
      f"({len(objs - set(code))} objects are assembly or absent: {', '.join(sorted(objs - set(code)))})",
      file=sys.stderr)
