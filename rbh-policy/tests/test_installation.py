#!/usr/bin/env python3.9

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import os
import sys
import importlib.util

SITE_PACKAGES_DIR = '/lib/python3.9/site-packages'
SCRIPT_PATH = '/usr/bin/rbh-policy'
PACKAGE_NAME = 'rbhpolicy'

def is_package_installed():
    return os.path.isdir(os.path.join(SITE_PACKAGES_DIR, PACKAGE_NAME))

def test_installed_files():
    expected_files = [
        'rbhpolicy/__init__.py',
        'rbhpolicy/config/__init__.py',
        'rbhpolicy/config/config_loader.py',
    ]

    missing = []
    for rel_path in expected_files:
        full_path = os.path.join(SITE_PACKAGES_DIR, rel_path)
        if not os.path.isfile(full_path):
            missing.append(full_path)

    if missing:
        print("Missing installed files:")
        for f in missing:
            print(f"  - {f}")
        sys.exit(1)
    else:
        print("All expected files are present.")

def test_import_module():
    try:
        import rbhpolicy.config.config_loader as cl
        print("Module imported successfully.")
    except ImportError as e:
        print(f"Failed to import module: {e}")
        sys.exit(1)

def test_script_exists():
    if os.path.isfile(SCRIPT_PATH) and os.access(SCRIPT_PATH, os.X_OK):
        print("Main script is present and executable.")
    else:
        print(f"Main script is missing or not executable: {SCRIPT_PATH}")
        sys.exit(1)

if __name__ == '__main__':
    if not is_package_installed():
        print(f"Package '{PACKAGE_NAME}' is not installed. Skipping test.")
        sys.exit(0)
    test_installed_files()
    test_import_module()
    test_script_exists()
