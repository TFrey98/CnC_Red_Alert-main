#!/usr/bin/env python3
"""
gen-instantiations.py <linkdir> -- add explicit instantiations for every
template specialisation the last link-census reported missing.

The engine defines template members in .CPP files; standard C++ instantiates
them only for uses inside that file (Watcom did it automatically). Each
defining file ends with a port block of `template class X<T>;` lines. This
script merges newly reported specialisations into those blocks, so the list
always equals what the link actually needs. Rerun link-census.sh afterwards.
"""
import io, json, os, re, sys
LINK = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
census = json.load(open(os.path.join(LINK, 'census.json')))
WHERE = {'VectorClass': 'CODE/VECTOR.CPP', 'DynamicVectorClass': 'CODE/DYNAVEC.CPP',
         'TFixedIHeapClass': 'CODE/HEAP.CPP', 'CCPtr': 'CODE/CCPTR.CPP', 'MixFileClass': 'CODE/MIXFILE.CPP'}
need = {}
for sym in census.get('TEMPLATE', []):
    m = re.match(r'^(\w+)<(.*?)>::', sym)
    if m:
        need.setdefault(m.group(1), set()).add(m.group(2).replace('const*', 'const *'))
for t, args in need.items():
    if t not in WHERE:
        print(f"  no known definition file for template {t}: {sorted(args)}"); continue
    path = WHERE[t]
    s = io.open(path, encoding='latin-1').read()
    have = set(re.findall(r'^template class ' + t + r'<(.*)>;\s*$', s, re.M))
    add = sorted(a for a in args if a not in have)
    if add:
        s = s.rstrip('\n') + '\n' + '\n'.join(f'template class {t}<{a}>;' for a in add) + '\n'
        io.open(path, 'w', encoding='latin-1').write(s)
    print(f"  {path}: +{len(add)} {t} specialisations ({len(have) + len(add)} total)")
