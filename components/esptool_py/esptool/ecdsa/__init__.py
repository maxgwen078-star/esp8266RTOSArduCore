# Auto-injected sys.path fix (portable build)
import os as _os_syspath_patch
import sys as _sys_syspath_patch
_HERE_SYSPATH_PATCH = _os_syspath_patch.path.dirname(_os_syspath_patch.path.abspath(__file__))
if _HERE_SYSPATH_PATCH not in _sys_syspath_patch.path:
    _sys_syspath_patch.path.insert(0, _HERE_SYSPATH_PATCH)
# End of auto-injected sys.path fix
__all__ = ["curves", "der", "ecdsa", "ellipticcurve", "keys", "numbertheory",
           "test_pyecdsa", "util", "six"]
from .keys import SigningKey, VerifyingKey, BadSignatureError, BadDigestError
from .curves import NIST192p, NIST224p, NIST256p, NIST384p, NIST521p, SECP256k1

_hush_pyflakes = [SigningKey, VerifyingKey, BadSignatureError, BadDigestError,
                  NIST192p, NIST224p, NIST256p, NIST384p, NIST521p, SECP256k1]
del _hush_pyflakes

# This code comes from http://github.com/warner/python-ecdsa

from ._version import get_versions
__version__ = get_versions()['version']
del get_versions
