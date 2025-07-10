#!/usr/bin/env python3.9

'''
This file is part of RobinHood 4
Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                   alternatives
SPDX-License-Identifer: LGPL-3.0-or-later
'''

import os
import tempfile
import shutil
from pathlib import Path

from rbhpolicy.config.config_loader import load_config, \
        CONFIG_DIR as ORIGINAL_CONFIG_DIR

def setup_test_environment():
    temp_dir = tempfile.mkdtemp()
    test_config_dir = os.path.join(temp_dir, "robinhood4.d")
    os.makedirs(test_config_dir)
    return temp_dir, test_config_dir

def create_file(path, content):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        f.write(content)

def test_file_config(test_config_dir):
    create_file(
        os.path.join(test_config_dir, "fs_file.py"), 'VAR = "rbh"\n'
        'def hello():\n    return "Hello file"\n'
    )
    load_config("fs_file")
    assert VAR == "rbh"
    assert hello() == "Hello file"

def test_directory_config(test_config_dir):
    dir_path = os.path.join(test_config_dir, "fs_dir")
    create_file(os.path.join(dir_path, "a.py"), 'A = 1\n')
    create_file(os.path.join(dir_path, "b.py"), 'B = 2\n')
    load_config("fs_dir")
    assert A == 1
    assert B == 2

def test_directory_empty(test_config_dir):
    os.makedirs(os.path.join(test_config_dir, "fs_empty"))
    try:
        load_config("fs_empty")
        assert False, "Expected FileNotFoundError"
    except FileNotFoundError:
        pass

def test_both_file_and_directory(test_config_dir):
    create_file(os.path.join(test_config_dir, "fs_both.py"), 'A = 42\n')
    create_file(os.path.join(test_config_dir, "fs_both", "A.py"), 'A = 99\n')
    try:
        load_config("fs_both")
        assert False, "Expected RuntimeError"
    except RuntimeError:
        pass

def test_no_config(test_config_dir):
    try:
        load_config("fs_none")
        assert False, "Expected FileNotFoundError"
    except FileNotFoundError:
        pass

def run_all_tests():
    temp_dir, test_config_dir = setup_test_environment()

    import rbhpolicy.config.config_loader as config_loader
    config_loader.CONFIG_DIR = test_config_dir

    # Liste des tests à exécuter
    tests = [
        ("test_file_config", test_file_config),
        ("test_directory_config", test_directory_config),
        ("test_directory_empty", test_directory_empty),
        ("test_both_file_and_directory", test_both_file_and_directory),
        ("test_no_config", test_no_config),
    ]

    passed = []
    failed = []

    try:
        for name, func in tests:
            print(f"\n==> Running {name}")
            try:
                func(test_config_dir)
                print(f"[PASS] {name}")
                passed.append(name)
            except AssertionError as e:
                print(f"[FAIL] {name} - AssertionError: {e}")
                failed.append(name)
            except Exception as e:
                print(f"[FAIL] {name} - {type(e).__name__}: {e}")
                failed.append(name)

        if failed:
            print("Failed tests:")
            for name in failed:
                print(f"  - {name}")
        else:
            print("All tests passed successfully.")

    finally:
        shutil.rmtree(temp_dir)
        config_loader.CONFIG_DIR = ORIGINAL_CONFIG_DIR

if __name__ == "__main__":
    run_all_tests()
