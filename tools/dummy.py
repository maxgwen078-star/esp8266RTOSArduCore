#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Dummy sink for Arduino IDE recipes (silent version).

Arduino IDE 2.x calls this script as a stand-in for the C/C++ compiler,
preprocessor, linker, etc. The real compilation is delegated to idf.py /
CMake via the postbuild hooks in platform.txt.

What matters:
  - When invoked with `-o <path>`, create that file (with a harmless
    placeholder) so that the ctags-based function prototype generator
    can open `sketch_merged.cpp`.
  - Do NOT emit anything on stdout, to keep the Arduino console clean.
"""
from __future__ import print_function
import os
import sys

PLACEHOLDER = "// dummy preprocessing placeholder\n"


def _ensure_parent_dir(path):
    parent = os.path.dirname(path)
    if parent and not os.path.isdir(parent):
        try:
            os.makedirs(parent)
        except OSError:
            pass


def _find_output_path(argv):
    """Detect '-o <path>' or '-o<path>' in the argument list."""
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '-o' and i + 1 < len(argv):
            return argv[i + 1]
        if arg.startswith('-o') and len(arg) > 2:
            return arg[2:]
        i += 1
    return None


def main():
    out_path = _find_output_path(sys.argv[1:])
    if out_path and out_path.lower() != 'nul':
        try:
            _ensure_parent_dir(out_path)
            with open(out_path, 'w') as f:
                f.write(PLACEHOLDER)
        except Exception:
            # Never fail: dummy must always exit 0
            pass
    return 0


if __name__ == '__main__':
    sys.exit(main())