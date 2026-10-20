# Copyright 2018 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# Base class for protocomm transport

# Auto-injected sys.path fix (portable build)
import os as _os_syspath_patch
import sys as _sys_syspath_patch
_HERE_SYSPATH_PATCH = _os_syspath_patch.path.dirname(_os_syspath_patch.path.abspath(__file__))
if _HERE_SYSPATH_PATCH not in _sys_syspath_patch.path:
    _sys_syspath_patch.path.insert(0, _HERE_SYSPATH_PATCH)
# End of auto-injected sys.path fix
import abc


class Transport():

    @abc.abstractmethod
    def send_session_data(self, data):
        pass

    @abc.abstractmethod
    def send_config_data(self, data):
        pass
