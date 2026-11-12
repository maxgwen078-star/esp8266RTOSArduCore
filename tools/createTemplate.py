# -*- coding: utf-8 -*-
from __future__ import print_function
import os as _os_syspath_patch
import sys as _sys_syspath_patch
_HERE_SYSPATH_PATCH = _os_syspath_patch.path.dirname(_os_syspath_patch.path.abspath(__file__))
if _HERE_SYSPATH_PATCH not in _sys_syspath_patch.path:
    _sys_syspath_patch.path.insert(0, _HERE_SYSPATH_PATCH)
import os
import sys
import argparse
import shutil
import glob
import re
import io


def copy_folder(source, destination):
    if not os.path.exists(destination):
        os.makedirs(destination)
    for item in os.listdir(source):
        s = os.path.join(source, item)
        d = os.path.join(destination, item)
        if os.path.isdir(s):
            copy_folder(s, d)
        else:
            shutil.copy2(s, d)


def move_sketch_files(source, destination):
    for ext in ['cpp', 'c', 'S', 'h', 'hpp', 'ino']:
        files = glob.glob(os.path.join(source, "*.{}".format(ext)))
        for file in files:
            dest_file = os.path.join(destination, os.path.basename(file))
            if os.path.exists(dest_file):
                os.remove(dest_file)
            shutil.move(file, destination)


def _normalize_name(name):
    """Retire espaces, underscores et tirets, met en minuscules."""
    return name.lower().replace(' ', '').replace('_', '').replace('-', '')


def _read_library_props(lib_dir):
    props_path = os.path.join(lib_dir, 'library.properties')
    props = {}
    if not os.path.isfile(props_path):
        return props
    try:
        with io.open(props_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#') or '=' not in line:
                    continue
                key, value = line.split('=', 1)
                props[key.strip()] = value.strip()
    except Exception:
        pass
    return props


def _find_lib_by_manifest_name(target_name, search_paths):
    """Trouve une bibliothèque par son nom déclaré dans library.properties."""
    target = _normalize_name(target_name)
    for base in search_paths:
        if not os.path.isdir(base):
            continue
        try:
            for entry in os.listdir(base):
                lib_dir = os.path.join(base, entry)
                if not os.path.isdir(lib_dir):
                    continue
                props = _read_library_props(lib_dir)
                manifest_name = props.get('name', '')
                if manifest_name and _normalize_name(manifest_name) == target:
                    return lib_dir
                if _normalize_name(entry) == target:
                    return lib_dir
        except OSError:
            pass
    return None


def _find_library_for_header(header_path, search_paths):
    root_header = header_path.split('/')[0] if '/' in header_path else header_path
    for base_path in search_paths:
        if not os.path.exists(base_path):
            continue
        try:
            for lib_name in os.listdir(base_path):
                lib_full_path = os.path.join(base_path, lib_name)
                if not os.path.isdir(lib_full_path):
                    continue
                src_path = os.path.join(lib_full_path, 'src')
                if os.path.exists(src_path):
                    if os.path.exists(os.path.join(src_path, header_path)) or \
                       os.path.exists(os.path.join(src_path, root_header)):
                        return lib_full_path
                if os.path.exists(os.path.join(lib_full_path, header_path)) or \
                   os.path.exists(os.path.join(lib_full_path, root_header)):
                    return lib_full_path
        except OSError:
            pass
    return None


def _expand_with_dependencies(initial_libs, search_paths):
    """Résout transitivement les depends= en normalisant les noms."""
    resolved = set()
    queue = list(initial_libs)
    while queue:
        lib_dir = os.path.normpath(os.path.abspath(queue.pop(0)))
        if lib_dir in resolved or not os.path.isdir(lib_dir):
            continue
        resolved.add(lib_dir)
        props = _read_library_props(lib_dir)
        depends_str = props.get('depends', '')
        if not depends_str:
            continue
        for dep_name in depends_str.split(','):
            dep_name = dep_name.strip()
            if not dep_name:
                continue
            dep_dir = _find_lib_by_manifest_name(dep_name, search_paths)
            if dep_dir and dep_dir not in resolved:
                queue.append(dep_dir)
    return resolved


def _process_library(lib_dir, include_dirs, sources):
    src_dir = os.path.join(lib_dir, 'src')
    inc_dir = src_dir if os.path.isdir(src_dir) else lib_dir
    include_dirs.add(inc_dir)
    for root, dirs, files in os.walk(inc_dir):
        dirs[:] = [d for d in dirs
                   if d.lower() not in ('examples', 'extras', 'test',
                                        'tests', 'docs', '.git')]
        for fname in files:
            abs_file = os.path.abspath(os.path.join(root, fname))
            if fname.endswith(('.c', '.cpp', '.S')):
                sources.add(abs_file)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--build', required=True)
    parser.add_argument('-i', '--idf', required=True)
    parser.add_argument('-x', '--xtensa', required=True)
    parser.add_argument('-l', '--libraries', required=True)
    parser.add_argument('-s', '--sketch', required=True)
    args = parser.parse_args()

    sep = ';' if os.name == 'nt' else ':'
    external_libs_paths = [
        os.path.expanduser(p.strip())
        for p in args.libraries.split(sep)
        if p.strip() and not p.startswith('{')
    ]

    sketchbook_path = os.path.dirname(os.path.normpath(args.sketch))
    sketchbook_libs = os.path.join(sketchbook_path, "libraries")
    if os.path.isdir(sketchbook_libs) and sketchbook_libs not in external_libs_paths:
        external_libs_paths.append(sketchbook_libs)

    env_user = os.environ.get('ARDUINO_DIRECTORIES_USER')
    if env_user:
        env_libs = os.path.join(env_user, 'libraries')
        if os.path.isdir(env_libs) and env_libs not in external_libs_paths:
            external_libs_paths.append(env_libs)

    internal_libs_path = os.path.join(args.idf, "libraries")
    search_paths = external_libs_paths + [internal_libs_path]

    source_template = os.path.join(args.idf, 'tools', 'idfTemplate')
    dest_template = os.path.join(args.build, 'idfTemplate')
    copy_folder(source_template, dest_template)

    sketch_source = os.path.join(args.build, 'sketch')
    main_component_dir = os.path.join(dest_template, 'main')
    move_sketch_files(sketch_source, main_component_dir)

    include_dirs = set()
    sources = set()
    used_libs = set()

    include_dirs.add(os.path.abspath(args.sketch))

    # Ajout des fichiers du croquis
    for ext in ['cpp', 'c', 'S', 'h', 'hpp', 'ino']:
        for f in glob.glob(os.path.join(main_component_dir, "*.{}".format(ext))):
            abs_f = os.path.abspath(f)
            if ext in ['cpp', 'c', 'S']:
                sources.add(abs_f)

    # On scanne uniquement les includes du sketch pour identifier les libs externes
    sketch_files_to_scan = []
    for ext in ['cpp', 'c', 'S', 'h', 'hpp', 'ino']:
        for f in glob.glob(os.path.join(main_component_dir, "*.{}".format(ext))):
            sketch_files_to_scan.append(os.path.abspath(f))
        for f in glob.glob(os.path.join(args.sketch, "*.{}".format(ext))):
            abs_f = os.path.abspath(f)
            if abs_f not in sketch_files_to_scan:
                sketch_files_to_scan.append(abs_f)

    processed_files = set()
    files_to_parse = list(sketch_files_to_scan)

    while files_to_parse:
        current_file = files_to_parse.pop(0)
        if current_file in processed_files:
            continue
        processed_files.add(current_file)

        if current_file.endswith('.ino.cpp') and main_component_dir in current_file:
            current_base_dir = args.sketch
        else:
            current_base_dir = os.path.dirname(current_file)

        try:
            with io.open(current_file, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    match = re.search(r'#include\s*[<"]([^>"]+)[>"]', line)
                    if not match:
                        continue
                    header_name = match.group(1)

                    local_path = os.path.abspath(os.path.join(current_base_dir, header_name))
                    if os.path.exists(local_path):
                        include_dirs.add(os.path.dirname(local_path))
                        if local_path not in processed_files and local_path not in files_to_parse:
                            files_to_parse.append(local_path)
                        continue

                    lib_dir = _find_library_for_header(header_name, external_libs_paths)
                    if lib_dir:
                        used_libs.add(lib_dir)
        except Exception:
            pass

    # Résolution des dépendances
    all_libs = _expand_with_dependencies(used_libs, search_paths)

    # Ajout des bibliothèques utilisées et de leurs dépendances
    for lib_dir in all_libs:
        _process_library(lib_dir, include_dirs, sources)

    # Ajout des bibliothèques internes de la plateforme (SPI, Wire, FS, etc.)
    if os.path.isdir(internal_libs_path):
        for name in os.listdir(internal_libs_path):
            lib_dir = os.path.join(internal_libs_path, name)
            if os.path.isdir(lib_dir):
                _process_library(lib_dir, include_dirs, sources)

    formatted_srcs = '\n    '.join(['"{}"'.format(s.replace(chr(92), "/")) for s in sources])
    formatted_dirs = '\n    '.join(['"{}"'.format(d.replace(chr(92), "/")) for d in include_dirs])

    cmake_content = """
set(LIBRARY_SRCS
    {0}
)

set(includedirs
    {1}
)

idf_component_register(SRCS "${{LIBRARY_SRCS}}" INCLUDE_DIRS "${{includedirs}}")

target_compile_definitions(${{COMPONENT_LIB}} PRIVATE 
    ESP8266=1
    ESP8266_RTOS=1
)
""".format(formatted_srcs, formatted_dirs)

    cmake_path = os.path.join(main_component_dir, "CMakeLists.txt")
    with open(cmake_path, "w") as f:
        f.write(cmake_content)


if __name__ == "__main__":
    main()