#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function
import io
import os
import re
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
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '-o' and i + 1 < len(argv):
            return argv[i + 1]
        if arg.startswith('-o') and len(arg) > 2:
            return arg[2:]
        i += 1
    return None


def _find_source_path(argv):
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '-c' and i + 1 < len(argv):
            return argv[i + 1]
        i += 1
    return None


def _find_depfile(argv):
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '-MF' and i + 1 < len(argv):
            return argv[i + 1]
        if arg.startswith('-MF') and len(arg) > 3:
            return arg[3:]
        i += 1
    return None


def _find_include_dirs(argv):
    dirs = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '-I' and i + 1 < len(argv):
            dirs.append(argv[i + 1])
            i += 2
            continue
        if arg.startswith('-I') and len(arg) > 2:
            dirs.append(arg[2:])
        i += 1
    return dirs


def _read_preferences_sketchbook(pref_file):
    try:
        with io.open(pref_file, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if line.startswith('sketchbook.path='):
                    return line.split('=', 1)[1].strip()
    except Exception:
        pass
    return None


def _read_arduino_cli_yaml(yaml_file):
    result = {}
    try:
        with io.open(yaml_file, 'r', encoding='utf-8', errors='ignore') as f:
            in_directories = False
            for raw in f:
                line = raw.rstrip('\n')
                stripped = line.strip()
                if not stripped or stripped.startswith('#'):
                    continue
                if stripped == 'directories:':
                    in_directories = True
                    continue
                if in_directories:
                    if not line.startswith(' ') and not line.startswith('\t'):
                        in_directories = False
                        continue
                    if ':' in stripped:
                        key, value = stripped.split(':', 1)
                        key = key.strip()
                        value = value.strip().strip('"').strip("'")
                        if key in ('data', 'user', 'downloads', 'builtin'):
                            result[key] = value
    except Exception:
        pass
    return result


def _iter_candidate_yaml_files():
    seen = set()
    def _yield(p):
        p = os.path.normpath(os.path.abspath(p))
        if p in seen:
            return
        seen.add(p)
        if os.path.isfile(p):
            yield p

    env_user = os.environ.get('ARDUINO_DIRECTORIES_USER')
    env_data = os.environ.get('ARDUINO_DIRECTORIES_DATA')
    candidates = []

    if env_data:
        base = os.path.dirname(os.path.normpath(env_data))
        candidates.append(os.path.join(base, 'settings', '.arduinoIDE', 'arduino-cli.yaml'))
        candidates.append(os.path.join(env_data, 'arduino-cli.yaml'))
    if env_user:
        base = os.path.dirname(os.path.normpath(env_user))
        candidates.append(os.path.join(base, 'settings', '.arduinoIDE', 'arduino-cli.yaml'))

    user_profile = os.environ.get('USERPROFILE') or os.path.expanduser('~')
    candidates.append(os.path.join(user_profile, '.arduinoIDE', 'arduino-cli.yaml'))
    appdata = os.environ.get('APPDATA')
    if appdata:
        candidates.append(os.path.join(appdata, 'Arduino IDE', 'arduino-cli.yaml'))

    for c in candidates:
        for y in _yield(c):
            yield y


def _read_sketchbook_paths():
    candidates = []

    env_user = os.environ.get('ARDUINO_DIRECTORIES_USER')
    if env_user:
        candidates.append(os.path.join(env_user, 'libraries'))
    env_sk = os.environ.get('ARDUINO_SKETCHBOOK_PATH')
    if env_sk:
        candidates.append(os.path.join(env_sk, 'libraries'))

    yaml_user_seen = False
    for yaml_file in _iter_candidate_yaml_files():
        dirs = _read_arduino_cli_yaml(yaml_file)
        if dirs.get('user'):
            candidates.append(os.path.join(dirs['user'], 'libraries'))
            yaml_user_seen = True
        if dirs.get('data'):
            candidates.append(os.path.join(dirs['data'], 'preferences.txt'))

    if not yaml_user_seen:
        pref_files = []
        env_data = os.environ.get('ARDUINO_DIRECTORIES_DATA')
        if env_data:
            pref_files.append(os.path.join(env_data, 'preferences.txt'))
        user_profile = os.environ.get('USERPROFILE') or os.path.expanduser('~')
        local_app = os.environ.get('LOCALAPPDATA') or os.path.join(user_profile, 'AppData', 'Local')
        pref_files.append(os.path.join(local_app, 'Arduino15', 'preferences.txt'))
        pref_files.append(os.path.join(user_profile, 'AppData', 'Local', 'Arduino15', 'preferences.txt'))

        for pref_file in pref_files:
            sk = _read_preferences_sketchbook(pref_file)
            if sk:
                candidates.append(os.path.join(sk, 'libraries'))

    candidates.append(os.path.join(os.path.expanduser('~'), 'Documents', 'Arduino', 'libraries'))
    candidates.append(os.path.join(os.path.expanduser('~'), 'Arduino', 'libraries'))

    seen = set()
    result = []
    for c in candidates:
        c_norm = os.path.normpath(os.path.abspath(os.path.expanduser(c)))
        if c_norm in seen:
            continue
        seen.add(c_norm)
        if os.path.isdir(c_norm):
            result.append(c_norm)

    # Ajout du dossier libraries de la plateforme si IDF_PATH est connu
    idf_path = os.environ.get('IDF_PATH')
    if idf_path:
        internal = os.path.join(idf_path, 'libraries')
        if os.path.isdir(internal):
            internal_norm = os.path.normpath(os.path.abspath(internal))
            if internal_norm not in seen:
                seen.add(internal_norm)
                result.append(internal_norm)

    return result


def _read_library_properties(lib_dir):
    props_path = os.path.join(lib_dir, 'library.properties')
    if not os.path.isfile(props_path):
        return {}
    props = {}
    try:
        with io.open(props_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#') or '=' not in line:
                    continue
                key, value = line.split('=', 1)
                key = key.strip()
                value = value.strip()
                if not value:
                    continue
                if key == 'depends':
                    props['depends'] = [d.strip() for d in value.split(',') if d.strip()]
                else:
                    props[key] = value
    except Exception:
        return {}
    return props


def _normalize_name(name):
    return name.lower().replace(' ', '').replace('_', '').replace('-', '')


def _find_lib_by_name(name, lib_roots):
    target = _normalize_name(name)
    for root in lib_roots:
        if not os.path.isdir(root):
            continue
        try:
            for entry in os.listdir(root):
                lib_dir = os.path.join(root, entry)
                if not os.path.isdir(lib_dir):
                    continue
                props = _read_library_properties(lib_dir)
                manifest = props.get('name', '').strip()
                if manifest and _normalize_name(manifest) == target:
                    return lib_dir
                if _normalize_name(entry) == target:
                    return lib_dir
        except OSError:
            pass
    return None


def _expand_library_deps(lib_roots):
    if not lib_roots:
        return []
    search_roots = list({os.path.dirname(os.path.normpath(r)) for r in lib_roots})
    search_roots = [r for r in search_roots if r and os.path.isdir(r)]

    seen = set()
    queue = [os.path.normpath(os.path.abspath(r)) for r in lib_roots]
    result = []
    while queue:
        lib_dir = queue.pop(0)
        if lib_dir in seen or not os.path.isdir(lib_dir):
            continue
        seen.add(lib_dir)
        result.append(lib_dir)
        props = _read_library_properties(lib_dir)
        for dep in props.get('depends', []):
            dep_dir = _find_lib_by_name(dep, search_roots + lib_roots)
            if dep_dir and dep_dir not in seen:
                queue.append(dep_dir)
    return result


def _expand_library_bases(lib_roots):
    bases = []
    seen = set()

    def _add(p):
        if not p:
            return
        p = os.path.normpath(os.path.abspath(p))
        if p in seen or not os.path.isdir(p):
            return
        seen.add(p)
        bases.append(p)

    for root in lib_roots:
        _add(root)
        try:
            for name in os.listdir(root):
                sub = os.path.join(root, name)
                if not os.path.isdir(sub):
                    continue
                _add(sub)
                src = os.path.join(sub, 'src')
                if os.path.isdir(src):
                    _add(src)
        except OSError:
            pass
    return bases


def _collect_includes(source_path, search_bases):
    if not source_path or not os.path.isfile(source_path):
        return []

    found = []
    seen = set()
    visited = set()

    def _resolve_one(inc, current_dir):
        cand = os.path.normpath(os.path.join(current_dir, inc))
        if os.path.isfile(cand):
            return cand
        for base in search_bases:
            cand = os.path.normpath(os.path.join(base, inc))
            if os.path.isfile(cand):
                return cand
        return None

    def _walk(path):
        path = os.path.normpath(os.path.abspath(path))
        if path in visited:
            return
        visited.add(path)
        if path not in seen:
            seen.add(path)
            found.append(path)
        cur_dir = os.path.dirname(path)
        try:
            with io.open(path, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    m = re.search(r'#\s*include\s*[<"]([^>"]+)[>"]', line)
                    if not m:
                        continue
                    inc = m.group(1).strip()
                    if not inc:
                        continue
                    resolved = _resolve_one(inc, cur_dir)
                    if resolved:
                        _walk(resolved)
        except Exception:
            pass

    _walk(source_path)
    return found[1:] if found else []


def _write_depfile(depfile, source_path, deps):
    try:
        _ensure_parent_dir(depfile)
        with io.open(depfile, 'w', encoding='utf-8', newline='\n') as f:
            src_abs = os.path.abspath(source_path) if source_path else ''
            if deps:
                lines = [src_abs + ': \\']
                for i, d in enumerate(deps):
                    suffix = ' \\' if i < len(deps) - 1 else ''
                    lines.append('  ' + os.path.abspath(d) + suffix)
                f.write('\n'.join(lines) + '\n')
            else:
                f.write(src_abs + ':\n')
    except Exception:
        pass


def main():
    argv = sys.argv[1:]

    out_path = _find_output_path(argv)
    if out_path and out_path.lower() != 'nul':
        try:
            _ensure_parent_dir(out_path)
            with open(out_path, 'w') as f:
                f.write(PLACEHOLDER)
        except Exception:
            pass

    depfile = _find_depfile(argv)
    src = _find_source_path(argv)
    if depfile and src:
        search_bases = []

        for d in _find_include_dirs(argv):
            if os.path.isdir(d):
                search_bases.append(os.path.normpath(os.path.abspath(d)))

        lib_roots = _read_sketchbook_paths()
        expanded_libs = _expand_library_deps(lib_roots)
        search_bases.extend(_expand_library_bases(expanded_libs))

        seen = set()
        ordered_bases = []
        for b in search_bases:
            if b not in seen:
                seen.add(b)
                ordered_bases.append(b)

        deps = _collect_includes(src, ordered_bases)
        _write_depfile(depfile, src, deps)

    return 0


if __name__ == '__main__':
    sys.exit(main())