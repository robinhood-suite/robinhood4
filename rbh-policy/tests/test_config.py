#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import os
import tempfile
import shutil
from pathlib import Path
import unittest

from rbhpolicy.config.config_loader import load_config, \
        CONFIG_DIR as ORIGINAL_CONFIG_DIR

class ConfigLoaderTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.test_config_dir = os.path.join(self.temp_dir, "robinhood4.d")
        os.makedirs(self.test_config_dir)

        import rbhpolicy.config.config_loader as config_loader
        self.config_loader = config_loader
        self.config_loader.CONFIG_DIR = self.test_config_dir

    def tearDown(self):
        shutil.rmtree(self.temp_dir)
        self.config_loader.CONFIG_DIR = ORIGINAL_CONFIG_DIR

    def create_file(self, path, content):
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w") as f:
            f.write(content)

    def test_file_config(self):
        self.create_file(
            os.path.join(self.test_config_dir, "fs_file.py"),
            'VAR = "rbh"\n'
            'def hello():\n    return "Hello file"\n'
        )
        load_config("fs_file")
        self.assertEqual(VAR, "rbh", msg="VAR should be 'rbh' after loading "
                         "fs_file.py")
        self.assertEqual(hello(), "Hello file", msg="hello() should return "
                         "'Hello file' after loading fs_file.py")

    def test_directory_config(self):
        dir_path = os.path.join(self.test_config_dir, "fs_dir")
        self.create_file(os.path.join(dir_path, "a.py"), 'A = 1\n')
        self.create_file(os.path.join(dir_path, "b.py"), 'B = 2\n')
        load_config("fs_dir")
        self.assertEqual(A, 1, msg="A should be 1 after loading fs_dir/a.py")
        self.assertEqual(B, 2, msg="B should be 2 after loading fs_dir/b.py")

    def test_directory_empty(self):
        os.makedirs(os.path.join(self.test_config_dir, "fs_empty"))
        with self.assertRaises(FileNotFoundError, msg="Loading an empty "
                "directory should raise FileNotFoundError"):
            load_config("fs_empty")

    def test_both_file_and_directory(self):
        self.create_file(os.path.join(self.test_config_dir, "fs_both.py"),
                         'A = 42\n')
        self.create_file(os.path.join(self.test_config_dir, "fs_both", "A.py"),
                         'A = 99\n')
        with self.assertRaises(RuntimeError, msg="Loading both fs_both.py and "
                "fs_both/ should raise RuntimeError"):
            load_config("fs_both")

    def test_no_config(self):
        with self.assertRaises(FileNotFoundError, msg="Loading a non-existent "
                "config should raise FileNotFoundError"):
            load_config("fs_none")

if __name__ == "__main__":
    unittest.main(verbosity=2)
