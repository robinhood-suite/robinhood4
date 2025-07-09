#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import importlib
import sys
import os

CONFIG_DIR = "/etc/robinhood4.d"

def load_config(fs_input):
    """
    Load a configuration module for the given filesystem input.

    Behavior:
    - If fs_input is a simple name (no slashes, e.g. "fs1"), the file
      /etc/robinhood4.d/<fs_input>.py is looked up and imported.
    - If fs_input is an absolute path (e.g. "/tmp/fs1"), the file
      /tmp/fs1.py is looked up and imported.
    - If fs_input contains a slash but is not absolute (relative path),
      a ValueError is raised.

    The configuration file is imported dynamically using its name. It may
    internally perform its own imports if the user wishes to split the
    configuration across multiple files. In that case, the user is responsible
    for managing those imports within <fs_name>.py.

    If the file <fs_name>.py does not exist a FileNotFoundError is raised with a
    clear message.
    """

    if os.path.isabs(fs_input):
        config_dir = os.path.dirname(fs_input) or "/"
        module_name = os.path.basename(fs_input)
        config_path = os.path.join(config_dir, f"{module_name}.py")
    elif "/" in fs_input:
        raise ValueError(f"Relative paths are not allowed: '{fs_input}'")
    else:
        config_dir = CONFIG_DIR
        module_name = fs_input
        config_path = os.path.join(config_dir, f"{module_name}.py")

    if not os.path.isfile(config_path):
        raise FileNotFoundError(f"Configuration file not found: {config_path}")

    sys.path.insert(0, config_dir)
    try:
        return importlib.import_module(module_name)
    finally:
        sys.path.pop(0)

