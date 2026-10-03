#!/usr/bin/env python3
"""
check-dup-headers.py -- fail if any library header that exists both in a module
directory and in its library's INCLUDE/ directory has drifted.

Module sources pick up their own directory's copy (quoted include); the game
picks up INCLUDE/. If the two differ, the library and the game can disagree on a
class layout -- a one-definition-rule violation that only fails at run time. It
also caused real mistakes during the port: a fix applied to one copy only.
"""
import glob, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
norm = lambda p: open(p, 'rb').read().replace(b'\r', b'').replace(b'\x1a', b'').rstrip()
bad = []
for lib, incdir in (('WIN32LIB', 'WIN32LIB/INCLUDE'), ('WINVQ', 'WINVQ/INCLUDE')):
    idx = {}
    for root, _, fs in os.walk(incdir):
        for f in fs:
            if f.upper().endswith('.H'):
                idx.setdefault(f.upper(), []).append(os.path.join(root, f))
    for d in glob.glob(lib + '/*/'):
        if os.path.normpath(d) == os.path.normpath(incdir) or os.path.basename(os.path.normpath(d)) in ('VQAVIEW', 'VPLAY32'):
            continue
        for f in os.listdir(d):
            for other in idx.get(f.upper(), []):
                if norm(os.path.join(d, f)) != norm(other):
                    bad.append((os.path.join(d, f), other))
for a, b in bad:
    print(f"DRIFT: {a}  !=  {b}")
print(f"duplicate library headers: {'all identical' if not bad else str(len(bad)) + ' drifted'}")
sys.exit(1 if bad else 0)
