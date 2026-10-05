#!/bin/zsh
#
# setup.sh -- the one tool the assembly reference needs that macOS lacks: the
# Unicorn CPU emulator, installed into port/asmref/.venv (git-ignored). Only
# needed to REGENERATE port/tests/asm_vectors; port/tests/run.sh does not.
#
#   port/asmref/setup.sh
#   port/asmref/.venv/bin/python port/asmref/gen_vectors.py [routine ...]
set -e
D="$(cd "$(dirname "$0")" && pwd)"
[[ -x "$D/.venv/bin/python" ]] || python3 -m venv "$D/.venv"
"$D/.venv/bin/pip" install -q 'unicorn==2.1.4'
"$D/.venv/bin/python" -c "import unicorn; print('unicorn', unicorn.__version__, 'ready')"
