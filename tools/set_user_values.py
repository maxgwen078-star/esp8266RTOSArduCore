# -*- coding: utf-8 -*-
from __future__ import print_function
import os as _os_syspath_patch
import sys as _sys_syspath_patch
_HERE_SYSPATH_PATCH = _os_syspath_patch.path.dirname(_os_syspath_patch.path.abspath(__file__))
if _HERE_SYSPATH_PATCH not in _sys_syspath_patch.path:
    _sys_syspath_patch.path.insert(0, _HERE_SYSPATH_PATCH)
import os
import sys
import shutil
import argparse


PARTITION_SCHEMES = {
    "fs2mb":  "partitions_4MB_fs2mb.csv",
    "fs3mb":  "partitions_4MB_fs3mb.csv",
    "fs1mb":  "partitions_4MB_fs1mb.csv",
    "fsnone": "partitions_4MB_fsnone.csv",
}


def _safe_rmtree(path):
    """Supprime un dossier de façon robuste (Windows-friendly)."""
    if not os.path.isdir(path):
        return False
    try:
        shutil.rmtree(path)
        return True
    except Exception as e:
        print("[Config Injector] WARNING: cannot remove {}: {}".format(path, e),
              file=sys.stderr)
        return False


def main():
    parser = argparse.ArgumentParser(description='Inject Arduino IDE hardware settings into ESP-IDF sdkconfig and CMake')
    parser.add_argument('-b',  '--build', required=True)
    parser.add_argument('-i',  '--idf', required=False, default=None)
    parser.add_argument('-c',  '--cpu_freq', required=True)
    parser.add_argument('-fm', '--flash_mode', required=True)
    parser.add_argument('-ff', '--flash_freq', required=True)
    parser.add_argument('-fs', '--flash_size', required=True)
    parser.add_argument('-l',  '--log_level', required=True)
    parser.add_argument('-v',  '--variant', required=True)
    parser.add_argument('-ps', '--partition_scheme', required=False, default=None)
    parser.add_argument('-e',  '--extra_flags', nargs=argparse.REMAINDER, default=[])

    args, unknown = parser.parse_known_args()

    if not args.idf:
        args.idf = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    idf_template_dir = os.path.join(args.build, "idfTemplate")
    sdkconfig_path   = os.path.join(idf_template_dir, "sdkconfig")
    cmakelists_path  = os.path.join(idf_template_dir, "CMakeLists.txt")
    build_dir        = os.path.join(idf_template_dir, "build")
    stamp_path       = os.path.join(idf_template_dir, ".build_stamp")

    if not os.path.exists(sdkconfig_path):
        print("[Config Injector] Error: sdkconfig not found", file=sys.stderr)
        sys.exit(1)

    print("[Config Injector] Translating Arduino IDE settings to ESP-IDF Kconfig...", file=sys.stderr)
    print("[Config Injector]   flash_size='{}'  partition_scheme='{}'".format(args.flash_size, args.partition_scheme), file=sys.stderr)
    print("[Config Injector]   idf_path='{}'".format(args.idf), file=sys.stderr)

    # ------------------------------------------------------------------
    # Détection de changement critique -> invalide le cache de build
    # ------------------------------------------------------------------
    extra_flags_clean = " ".join(args.extra_flags).strip()
    if extra_flags_clean == '""' or extra_flags_clean == "''":
        extra_flags_clean = ""

    stamp_data = "|".join([
        "ps=" + (args.partition_scheme or ""),
        "fs=" + args.flash_size,
        "fm=" + args.flash_mode,
        "ff=" + args.flash_freq,
        "cpu=" + args.cpu_freq,
        "ef=" + extra_flags_clean,
        "log=" + str(args.log_level),
    ])

    old_stamp = ""
    if os.path.exists(stamp_path):
        try:
            with open(stamp_path, "r") as f:
                old_stamp = f.read().strip()
        except Exception:
            old_stamp = ""

    if old_stamp and old_stamp != stamp_data:
        print("[Config Injector] Critical config changed -> purging build dir",
              file=sys.stderr)
        print("[Config Injector]   old: " + old_stamp, file=sys.stderr)
        print("[Config Injector]   new: " + stamp_data, file=sys.stderr)
        _safe_rmtree(build_dir)

    try:
        with open(stamp_path, "w") as f:
            f.write(stamp_data)
    except Exception as e:
        print("[Config Injector] WARNING: cannot write stamp: {}".format(e),
              file=sys.stderr)

    # ------------------------------------------------------------------
    # Gestion unifiée des niveaux de log
    # ------------------------------------------------------------------
    # args.log_level peut être : NONE, ERROR, WARN, INFO, DEBUG, VERBOSE
    # (ou 0..5 selon le format de boards.txt)
    log_level_str = str(args.log_level).strip().upper()

    LOG_LEVELS = {
        "NONE":    (0, "NONE"),
        "ERROR":   (1, "ERROR"),
        "WARN":    (2, "WARN"),
        "INFO":    (3, "INFO"),
        "DEBUG":   (4, "DEBUG"),
        "VERBOSE": (5, "VERBOSE"),
        # Variantes numériques
        "0": (0, "NONE"),
        "1": (1, "ERROR"),
        "2": (2, "WARN"),
        "3": (3, "INFO"),
        "4": (4, "DEBUG"),
        "5": (5, "VERBOSE"),
    }

    log_num, log_name = LOG_LEVELS.get(log_level_str, (3, "INFO"))
    print("[Config Injector] Log level: {} (num={})".format(log_name, log_num),
          file=sys.stderr)

    # Construire les mises à jour pour les 3 systèmes de log
    log_updates = {}

    # --- 1) Core Arduino : CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL ---
    log_updates["CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL"] = str(log_num)

    # --- 2) ESP-IDF : CONFIG_LOG_DEFAULT_LEVEL + choix du niveau ---
    for lvl in ["NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"]:
        key = "CONFIG_LOG_DEFAULT_LEVEL_{}".format(lvl)
        log_updates[key] = "y" if lvl == log_name else None
    log_updates["CONFIG_LOG_DEFAULT_LEVEL"] = str(log_num)

    # --- 3) Bootloader : CONFIG_LOG_BOOTLOADER_LEVEL + choix du niveau ---
    for lvl in ["NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"]:
        key = "CONFIG_LOG_BOOTLOADER_LEVEL_{}".format(lvl)
        log_updates[key] = "y" if lvl == log_name else None
    log_updates["CONFIG_LOG_BOOTLOADER_LEVEL"] = str(log_num)


    # ------------------------------------------------------------------
    # Construction de config_updates
    # ------------------------------------------------------------------
    config_updates = {
        # --- Logs (les 3 niveaux synchronisés) ---
        **log_updates,

        # --- Console UART baudrate ---
        "CONFIG_ESP_CONSOLE_UART_BAUDRATE": "115200",

        # --- Monitor baud rate ---
        "CONFIG_ESPTOOLPY_MONITOR_BAUD_74880B":    None,
        "CONFIG_ESPTOOLPY_MONITOR_BAUD_115200B":   "y",
        "CONFIG_ESPTOOLPY_MONITOR_BAUD_OTHER_VAL": "115200",
        "CONFIG_ESPTOOLPY_MONITOR_BAUD":           "115200",       
    }

    # ------------------------------------------------------------------
    # CPU frequency
    # ------------------------------------------------------------------
    if "160" in args.cpu_freq:
        config_updates["CONFIG_ESP8266_DEFAULT_CPU_FREQ_160"] = "y"
        config_updates["CONFIG_ESP8266_DEFAULT_CPU_FREQ_80"] = None
    else:
        config_updates["CONFIG_ESP8266_DEFAULT_CPU_FREQ_80"] = "y"
        config_updates["CONFIG_ESP8266_DEFAULT_CPU_FREQ_160"] = None

    # ------------------------------------------------------------------
    # Flash size
    # ------------------------------------------------------------------
    for s in ["1MB", "2MB", "4MB", "8MB", "16MB"]:
        key = "CONFIG_ESPTOOLPY_FLASHSIZE_{}".format(s)
        config_updates[key] = "y" if s == args.flash_size else None

    # ------------------------------------------------------------------
    # Flash mode
    # ------------------------------------------------------------------
    for m_key, m_val in {"qio": "QIO", "qout": "QOUT", "dio": "DIO", "dout": "DOUT"}.items():
        key = "CONFIG_ESPTOOLPY_FLASHMODE_{}".format(m_val)
        config_updates[key] = "y" if m_key == args.flash_mode.lower() else None

    # ------------------------------------------------------------------
    # Flash frequency
    # ------------------------------------------------------------------
    for f_key, f_val in {"40": "40M", "80": "80M"}.items():
        key = "CONFIG_ESPTOOLPY_FLASHFREQ_{}".format(f_val)
        config_updates[key] = "y" if f_key in args.flash_freq else None

    # ------------------------------------------------------------------
    # Partition scheme
    # ------------------------------------------------------------------
    scheme_key = (args.partition_scheme or "").strip().lower()
    csv_name = PARTITION_SCHEMES.get(scheme_key)

    if csv_name:
        csv_src = os.path.join(args.idf, "components", "partition_table", csv_name)
        if os.path.exists(csv_src):
            csv_dst = os.path.join(idf_template_dir, csv_name)
            try:
                shutil.copy(csv_src, csv_dst)
                print("[Config Injector] Copied {} -> {}".format(csv_name, csv_dst),
                      file=sys.stderr)
            except Exception as e:
                print("[Config Injector] WARNING: cannot copy CSV: {}".format(e),
                      file=sys.stderr)

            config_updates["CONFIG_PARTITION_TABLE_SINGLE_APP"] = None
            config_updates["CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE"] = None
            config_updates["CONFIG_PARTITION_TABLE_TWO_OTA"] = None
            config_updates["CONFIG_PARTITION_TABLE_FACTORY"] = None
            config_updates["CONFIG_PARTITION_TABLE_CUSTOM"] = "y"
            config_updates["CONFIG_PARTITION_TABLE_CUSTOM_FILENAME"] = '"{}"'.format(csv_name)
            config_updates["CONFIG_PARTITION_TABLE_FILENAME"] = '"{}"'.format(csv_name)

            print("[Config Injector] Partition scheme: {} -> {}".format(scheme_key, csv_name),
                  file=sys.stderr)
        else:
            print("[Config Injector] WARNING: CSV not found: {}".format(csv_src),
                  file=sys.stderr)
    else:
        print("[Config Injector] Partition scheme: (not set) - using SDK default",
              file=sys.stderr)

    # ------------------------------------------------------------------
    # Injection sdkconfig
    # ------------------------------------------------------------------
    with open(sdkconfig_path, "r") as f:
        lines = f.readlines()

    new_lines = []
    keys_to_modify = list(config_updates.keys())

    for line in lines:
        stripped_line = line.strip()
        keep_line = True
        for key in keys_to_modify:
            if stripped_line.startswith(key + "=") or stripped_line == "# {} is not set".format(key):
                keep_line = False
                break
        if keep_line:
            new_lines.append(line)

    new_lines.append("\n# --- Arduino IDE Injected Configurations ---\n")
    for key, value in config_updates.items():
        if value is not None:
            new_lines.append("{}={}\n".format(key, value))

    with open(sdkconfig_path, "w") as f:
        f.writelines(new_lines)

    # ------------------------------------------------------------------
    # Injection CMakeLists.txt
    # ------------------------------------------------------------------
    variant_clean = args.variant.strip()

    if os.path.exists(cmakelists_path):
        with open(cmakelists_path, "r") as f:
            cmake_lines = f.readlines()

        injection = []

        # --- Arduino loopTask stack size override ---
        print("[Config Injector] Forcing CONFIG_ARDUINO_LOOP_STACK_SIZE=1536 via CMake", file=sys.stderr)
        injection.extend([
            "\n# --- Arduino loopTask Stack Size Override ---\n", 
            'add_compile_options("-DCONFIG_ARDUINO_LOOP_STACK_SIZE=1536")\n', 
            "# --------------------------------------------\n\n"
        ])

        if variant_clean:
            print("[Config Injector] Linking board variant '{}'...".format(variant_clean),
                  file=sys.stderr)
            injection.extend([
                "\n# --- Arduino IDE Board Variant Linkage ---\n",
                'set(ARDUINO_VARIANT "{}" CACHE STRING "Arduino Board Variant" FORCE)\n'.format(variant_clean),
                'add_compile_options("-DARDUINO_VARIANT_{}")\n'.format(
                    variant_clean.upper().replace("-", "_").replace(".", "_")),
                "# -----------------------------------------------------\n\n"
            ])

        if extra_flags_clean:
            print("[Config Injector] Custom build.extra_flags detected!",
                  file=sys.stderr)
            injection.extend([
                "\n# --- Arduino IDE Custom build.extra_flags ---\n",
                'set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} %s")\n' % extra_flags_clean,
                'set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} %s")\n' % extra_flags_clean,
                "# -----------------------------------------------------\n\n"
            ])

        if injection:
            cmake_lines = injection + cmake_lines
            with open(cmakelists_path, "w") as f:
                f.writelines(cmake_lines)

    print("[Config Injector] Success! Configuration bridge fully synced.",
          file=sys.stderr)


if __name__ == "__main__":
    main()