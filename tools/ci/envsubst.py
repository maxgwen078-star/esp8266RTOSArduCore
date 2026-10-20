#!/usr/bin/env python
#
# A script similar to GNU envsubst, but filters out
# some CI related variables.

# Auto-injected sys.path fix (portable build)
import os as _os_syspath_patch
import sys as _sys_syspath_patch
_HERE_SYSPATH_PATCH = _os_syspath_patch.path.dirname(_os_syspath_patch.path.abspath(__file__))
if _HERE_SYSPATH_PATCH not in _sys_syspath_patch.path:
    _sys_syspath_patch.path.insert(0, _HERE_SYSPATH_PATCH)
# End of auto-injected sys.path fix
import os
import sys


def main():
    # Sanitize environment variables
    vars_to_remove = []
    for var_name in os.environ.keys():
        if var_name.startswith('CI_'):
            vars_to_remove.append(var_name)
    for var_name in vars_to_remove:
        del os.environ[var_name]

    for line in sys.stdin:
        if not line:
            break
        sys.stdout.write(os.path.expandvars(line))
    sys.stdout.flush()


if __name__ == '__main__':
    main()
