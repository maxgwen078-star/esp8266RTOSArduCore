# -*- coding: utf-8 -*-
from __future__ import print_function
import os
import sys
import argparse
import subprocess


# Keywords that mark a line as an actual compilation error (RED)
ERROR_KEYWORDS = (
    'error:',
    'Error:',
    'ERROR:',
    'FAILED:',
    'fatal error',
    'ninja: build stopped',
    'ninja: error',
    'Traceback',
    'undefined reference',
    'collect2: error',
)


def _is_error(line):
    return any(kw in line for kw in ERROR_KEYWORDS)


def run_with_color_routing(cmd, env, cwd):
    """
    Run a command, reading its output line by line.
    Since Arduino IDE 2.x does NOT interpret ANSI color codes on Windows,
    we rely on the native stream routing:
      - stderr -> red   (real compilation errors only)
      - stdout -> black (everything else, including warnings)
    CMake developer noise is filtered out entirely.
    """
    env["PYTHONUNBUFFERED"] = "1"

    process = subprocess.Popen(
        cmd, env=env, cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
        bufsize=1,
    )

    in_cmake_block = False

    # ---------- CMake noise ----------
    BLOCK_STARTERS = (
        'CMake Deprecation Warning',
        'CMake Warning (dev)',
        'CMake Warning:',
        'CMake Warning at',
    )
    BLOCK_ENDERS = (
        '-- ',
        '[',
        'ninja: ',
        'Running ',
        'Executing ',
        'Project build',
        'Generating ',
        'Linking ',
        'esptool.py',
        'Generated ',
    )
    SINGLE_SKIP = (
        'This warning is for project developers',
        'Compatibility with CMake < 3.5',
        'Update the VERSION argument',
        'Run "cmake --help-policy',
        'for policy details.',
        'command to set the policy',
        'Call Stack (most recent call first):',
        'Source file extensions must be explicit',
        'FindPythonInterp and FindPythonLibs modules',
        'are removed.  Run',
        'Manually-specified variables were not used by the project:',
        'fatal: not a git repository',
        "will not use 'git describe'",
        'Could NOT find Perl',
        '-- Configuring done',
        '-- Generating done',
        '-- Build files have been written to:',
        '-- Project version:',
        '-- Building ESP-IDF components',
        '-- Adding linker script',
        '-- Components:',
        '-- Component paths:',
        'Unable to cache built core',
    )

    def _is_block_starter(line):
        return any(kw in line for kw in BLOCK_STARTERS)

    def _is_block_ender(line):
        return any(kw in line for kw in BLOCK_ENDERS)

    def _is_single_skip(line):
        return any(kw in line for kw in SINGLE_SKIP)

    try:
        for line in iter(process.stdout.readline, ''):
            if not line:
                break

            stripped = line.lstrip()

            # 1. Skip CMake dev-noise blocks
            if in_cmake_block:
                if _is_block_ender(stripped):
                    in_cmake_block = False
                else:
                    continue

            # 2. Entering a CMake dev-noise block?
            if _is_block_starter(line):
                in_cmake_block = True
                continue

            # 3. Skip single-line CMake noise
            if _is_single_skip(line):
                continue

            # 4. Real compilation error -> stderr (red in Arduino IDE)
            if _is_error(line):
                sys.stderr.write(line)
                sys.stderr.flush()
                continue

            # 5. Everything else (including warnings) -> stdout (black)
            sys.stdout.write(line)
            sys.stdout.flush()
    finally:
        process.stdout.close()
        process.wait()

    return process.returncode


def main():
    parser = argparse.ArgumentParser(description='Windows Native Compiler Wrapper (portable)')
    parser.add_argument('-b', '--build', required=True)
    parser.add_argument('-i', '--idf', required=True)
    parser.add_argument('-x', '--xtensa', required=True)
    parser.add_argument('-m', '--max_size', required=False)
    parser.add_argument('-f', '--flash_size', required=False)
    parser.add_argument('-C', '--cmake-dir',  default=None)
    parser.add_argument('-G', '--gcc-dir',    default=None)
    parser.add_argument('-M', '--make-dir',   default=None)
    parser.add_argument('-N', '--ninja-dir',  default=None)
    parser.add_argument('-P', '--python-dir', default=None)
    args, unknown = parser.parse_known_args()

    work_dir = os.path.join(args.build, "idfTemplate")
    if not os.path.exists(work_dir):
        os.makedirs(work_dir)

    print("[Builder] Starting ESP8266 RTOS compilation (portable Windows)...",
          file=sys.stderr, flush=True)

    rtos_root = os.path.dirname(os.path.dirname(os.path.dirname(args.idf)))

    def resolve(arg_val, *fallback_parts):
        if arg_val:
            return arg_val
        return os.path.join(rtos_root, "tools", *fallback_parts)

    xtensa_bin = os.path.join(args.xtensa, "bin")
    cmake_dir  = resolve(args.cmake_dir,  "cmake")
    gcc_dir    = resolve(args.gcc_dir,    "gcc")
    make_dir   = resolve(args.make_dir,   "make")
    ninja_dir  = resolve(args.ninja_dir,  "ninja")
    python_dir = resolve(args.python_dir, "python3", "3.7.2-post1")

    cmake_bin = os.path.join(cmake_dir, "bin")
    ninja_bin = ninja_dir
    make_bin  = make_dir
    python_bin = python_dir

    native_gcc_bin = gcc_dir
    if not (os.path.isdir(native_gcc_bin) and
            os.path.isfile(os.path.join(native_gcc_bin, "gcc.exe"))):
        print("[Builder] WARNING: gcc.exe not found in " + native_gcc_bin,
              file=sys.stderr, flush=True)

    env = os.environ.copy()
    env["IDF_PATH"] = args.idf

    path_parts = [
        xtensa_bin,
        cmake_bin,
        ninja_bin,
        make_bin,
        python_bin,
        native_gcc_bin,
    ]
    path_parts.append(env.get("PATH", ""))

    env["PATH"] = os.pathsep.join(path_parts)
    env["CMAKE_POLICY_VERSION_MINIMUM"] = "3.5"

    idf_py = os.path.join(args.idf, "tools", "idf.py")

    print("[Builder] Portable tool paths:", file=sys.stderr)
    print("  xtensa: " + xtensa_bin, file=sys.stderr)
    print("  cmake : " + cmake_bin, file=sys.stderr)
    print("  ninja : " + ninja_bin, file=sys.stderr)
    print("  make  : " + make_bin, file=sys.stderr)
    print("  python: " + python_bin, file=sys.stderr)
    print("  gcc   : " + native_gcc_bin, file=sys.stderr)
    sys.stderr.flush()

    rc = run_with_color_routing([sys.executable, idf_py, "build"], env, work_dir)
    if rc != 0:
        sys.exit(rc)


if __name__ == "__main__":
    main()