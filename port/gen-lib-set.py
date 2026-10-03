#!/usr/bin/env python3
"""
gen-lib-set.py -- the C/C++ translation units of the libraries the game links,
from each library's own makefile, written to port/lib-build-set.txt.

WIN32LIB/<module>/MAKEFILE build the modules of win32lib.lib; WINVQ/VQA32 and
WINVQ/VQM32 build the movie player. WINVQ/VPLAY32 and WINVQ/VQAVIEW are
standalone tools, not linked into the game, and are skipped.
"""
import glob, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
TOOLS = {'WINVQ/VPLAY32', 'WINVQ/VQAVIEW'}

out = []
for mk in sorted(glob.glob('WIN32LIB/*/MAKEFILE') + glob.glob('WINVQ/*/MAKEFILE')):
    d = os.path.dirname(mk)
    if d in TOOLS:
        continue
    objs = {m.upper() for m in re.findall(r'([A-Za-z0-9_]+)\.OBJ', open(mk, encoding='latin-1').read(), re.I)}
    for f in sorted(os.listdir(d)):
        stem, ext = os.path.splitext(f)
        if ext.upper() in ('.CPP', '.C') and stem.upper() in objs:
            out.append(f'{d}/{f}')
open('port/lib-build-set.txt', 'w').write('\n'.join(out) + '\n')
print(f"{len(out)} library translation units -> port/lib-build-set.txt", file=sys.stderr)
