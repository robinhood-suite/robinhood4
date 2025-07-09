#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import importlib.util
import types
import sys
import os


RBH_CONFIG_PATH = os.environ.get("RBH_CONFIG_PATH", "/etc/robinhood4.d")
CONFIG_DIR = os.path.join(RBH_CONFIG_PATH, "policies")

def get_config_file_path(fs_name):
    return os.path.join(CONFIG_DIR, f"{fs_name}.py")

def get_config_dir_path(fs_name):
    return os.path.join(CONFIG_DIR, fs_name)

def load_config(fs_name):
    """
    Loads configuration for the given filesystem name.
    - If a file named <fs_name>.py exists, it is loaded as a module.
    - If a directory named <fs_name>/ exists, all .py files inside are loaded in
      alphabetical order.
    - If both exist, an error is raised.
    - If neither exists, a FileNotFoundError is raised.
    """
    file_path = get_config_file_path(fs_name)
    dir_path = get_config_dir_path(fs_name)

    file_exists = os.path.isfile(file_path)
    dir_exists = os.path.isdir(dir_path)

    if file_exists and dir_exists:
        raise RuntimeError(
            f"Ambiguous configuration for '{fs_name}': both file and directory"
            " exist.\n"
            f"  File: {file_path}\n"
            f"  Dir:  {dir_path}\n"
            "Please remove one of them to resolve the conflict."
        )

    config_module = types.ModuleType(f"config_{fs_name}")

    if file_exists:
        spec = importlib.util.spec_from_file_location(config_module.__name__,
                file_path)
        spec.loader.exec_module(config_module)

    elif dir_exists:
        py_files = sorted(
            f for f in os.listdir(dir_path)
            if f.endswith('.py') and os.path.isfile(os.path.join(dir_path, f))
        )

        if not py_files:
            raise FileNotFoundError(
                f"Configuration directory '{dir_path}' is empty.\n"
                "At least one .py file is required in the directory."
        )

        for fname in py_files:
            fpath = os.path.join(dir_path, fname)
            with open(fpath) as f:
                exec(f.read(), config_module.__dict__)

    else:
        raise FileNotFoundError(
            f"No configuration found for '{fs_name}'.\n"
            f"  Expected file: {file_path}\n"
            f"  Or directory:  {dir_path}"
        )

    caller_globals = sys._getframe(1).f_globals
    for key, value in config_module.__dict__.items():
        if not key.startswith("__"):
            caller_globals[key] = value
