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
from importlib import import_module

class ConfigLoaderTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.test_config_dir = os.path.join(self.temp_dir, "robinhood4.d")
        os.makedirs(self.test_config_dir)

        self.config_loader = import_module('rbhpolicy.config.config_loader')
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
        mod = self.config_loader.load_config("fs_file")
        self.assertEqual(mod.VAR, "rbh", msg="VAR should be 'rbh' after loading "
                         "fs_file.py")
        self.assertEqual(mod.hello(), "Hello file", msg="hello() should return "
                         "'Hello file' after loading fs_file.py")

    def test_file_config_missing(self):
        with self.assertRaises(FileNotFoundError, msg="Expected "
                "FileNotFoundError when loading non-existent config 'fs_missing'"
                ):
            self.config_loader.load_config("fs_missing")

    def test_absolute_path(self):
        abs_path = os.path.join(self.temp_dir, "fs2")
        self.create_file(abs_path + ".py",
                'X = 42\ndef hello(): return "hello"\n')
        mod = self.config_loader.load_config(abs_path)
        self.assertEqual(mod.X, 42, msg="X should be 42 after loading fs2.py "
                "via absolute path")
        self.assertEqual(mod.hello(), "hello", msg="hello() should return "
                "'hello' after loading fs2.py via absolute path")

    def test_absolute_path_missing(self):
        abs_path = os.path.join(self.temp_dir, "fs_missing")
        with self.assertRaises(FileNotFoundError, msg="Expected "
                "FileNotFoundError when loading missing absolute path config"):
            self.config_loader.load_config(abs_path)

    def test_relative_path_rejected(self):
        with self.assertRaises(ValueError, msg="Expected ValueError when "
                "loading config with relative path 'tmp/fs3'"):
            self.config_loader.load_config("tmp/fs3")

    def test_import_from_subdirectory(self):
        sub_path = os.path.join(self.test_config_dir, "fs_sub", "submodule.py")
        self.create_file(sub_path,
                'SUBVAR = "subvalue"\ndef subhello(): return "subhello"\n')

        main_path = os.path.join(self.test_config_dir, "fs_import.py")
        self.create_file(main_path, 'from fs_sub import submodule\n'
                                    'VAR = submodule.SUBVAR\n'
                                    'def hello(): return submodule.subhello()\n')

        mod = self.config_loader.load_config("fs_import")
        self.assertEqual(mod.VAR, "subvalue",
                msg="VAR should reflect submodule.SUBVAR")
        self.assertEqual(mod.hello(), "subhello",
                msg="hello() should return submodule.subhello()")

    def test_import_from_sibling_module(self):
        shared_path = os.path.join(self.test_config_dir, "shared.py")
        self.create_file(shared_path,
                         'SHARED = 123\ndef shared_func(): return "shared ok"\n')

        fs_path = os.path.join(self.test_config_dir, "fs1.py")
        self.create_file(fs_path, 'import shared\n'
                                  'VAR = shared.SHARED\n'
                                  'def hello(): return shared.shared_func()\n')

        mod = self.config_loader.load_config("fs1")
        self.assertEqual(mod.VAR, 123, msg="VAR should reflect shared.SHARED")
        self.assertEqual(mod.hello(), "shared ok",
                msg="hello() should return shared.shared_func()")

    def test_deep_cascading_imports(self):
        fileclass2_path = os.path.join(self.test_config_dir, "subdir2",
                                       "fileclass2.py")
        self.create_file(fileclass2_path,
                         'VAL2 = "deep"\n'
                         'def func2(): return "from fileclass2"\n')

        fileclass_path = os.path.join(self.test_config_dir, "subdir",
                                      "fileclass.py")
        self.create_file(fileclass_path,
                         'from subdir2 import fileclass2\n'
                         'VAL1 = fileclass2.VAL2\n'
                         'def func1(): return fileclass2.func2()\n')

        fs3_path = os.path.join(self.test_config_dir, "fs3.py")
        self.create_file(fs3_path,
                         'from subdir import fileclass\n'
                         'VAR = fileclass.VAL1\n'
                         'def hello(): return fileclass.func1()\n')

        mod = self.config_loader.load_config("fs3")
        self.assertEqual(mod.VAR, "deep",
                         msg="VAR should reflect fileclass2.VAL2")
        self.assertEqual(mod.hello(), "from fileclass2",
                         msg="hello() should return fileclass2.func2()")

if __name__ == "__main__":
    unittest.main(verbosity=2)
