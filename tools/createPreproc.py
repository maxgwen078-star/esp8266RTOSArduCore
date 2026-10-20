from __future__ import print_function
# Auto-injected sys.path fix (portable build)
import os as _os_syspath_patch
import sys as _sys_syspath_patch
_HERE_SYSPATH_PATCH = _os_syspath_patch.path.dirname(_os_syspath_patch.path.abspath(__file__))
if _HERE_SYSPATH_PATCH not in _sys_syspath_patch.path:
    _sys_syspath_patch.path.insert(0, _HERE_SYSPATH_PATCH)
# End of auto-injected sys.path fix
import argparse
import os
import sys


def main(build_path):
    print("preCompile Build Path: {}".format(build_path), file=sys.stderr)
    
    # Crear la carpeta 'preproc' y el archivo 'ctags_target_for_gcc_minus_e.cpp' en 'build_path'
    preproc_path = os.path.join(build_path, 'preproc')
    # PON ESTO EN SU LUGAR:
    if not os.path.exists(preproc_path):
        os.makedirs(preproc_path)

    file_path = os.path.join(preproc_path, 'ctags_target_for_gcc_minus_e.cpp')
    if not os.path.exists(file_path):
        with open(file_path, 'w') as f:
            pass

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Script that accepts build path as arguments.')
    parser.add_argument('-b', '--build', type=str, required=True, help='The build path.')
    args = parser.parse_args()

    main(args.build)
