# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import unittest
import ctypes
import os
import re
import sys
import tempfile
import shutil
import subprocess

build_lib_path = os.environ.get("BUILD_LIB_PATH")
if not build_lib_path:
    raise RuntimeError("[FATAL] BUILD_LIB_PATH not found in the environment")

librobinhood_so = os.path.join(build_lib_path, "librobinhood.so")
if not os.path.exists(librobinhood_so):
    raise RuntimeError(f"[FATAL] librobinhood.so not found in {build_lib_path}")

_original_cdll = ctypes.CDLL

def _patched_cdll(name, *args, **kwargs):
    if name == "librobinhood.so":
        return _original_cdll(librobinhood_so, *args, **kwargs)
    return _original_cdll(name, *args, **kwargs)

ctypes.CDLL = _patched_cdll

def get_build_root():
    return os.path.dirname(os.path.dirname(build_lib_path))

def find_rbh_sync():
    build_root = get_build_root()
    path = os.path.join(build_root, "rbh-sync", "rbh-sync")
    if not os.path.exists(path):
        raise RuntimeError(f"rbh-sync not found at expected location: {path}")
    return path

class BaseIntegrationTest(unittest.TestCase):
    BACKEND = None
    ROOT_PATH = None

    @classmethod
    def setUpClass(cls):
        if cls.BACKEND is None:
            raise RuntimeError("BACKEND must be defined in subclass")

        if cls.ROOT_PATH is None:
            raise RuntimeError("ROOT_PATH must be defined in subclass")

        if cls.BACKEND == "lustre":
            if not os.path.ismount(cls.ROOT_PATH):
                raise unittest.SkipTest("Lustre not mounted")
            plugin = os.path.join(build_lib_path, "plugins", "posix",
                                  "enrichers", "lustre",
                                  "librbh-posix-lustre-ext.so")
            if not os.path.exists(plugin):
                raise unittest.SkipTest("Lustre plugin missing")

    def setUp(self):
        self.test_dir = (
            tempfile.mkdtemp(prefix="rbh_test_")
            if self.BACKEND == "posix"
            else os.path.join(self.ROOT_PATH, f"rbh_test_{os.getpid()}")
        )
        if self.BACKEND == "lustre":
            os.makedirs(self.test_dir, exist_ok=True)

        self.mongo_db = f"test_{self.BACKEND}_{os.getpid()}"
        self.config_file_path = None

        self._create_filesystem_structure()
        self._sync_filesystem()

    def tearDown(self):
        if self.config_file_path and os.path.exists(self.config_file_path):
            os.remove(self.config_file_path)
        os.environ.pop("RBH_CONFIG_PATH", None)
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir, ignore_errors=True)

    def _sync_filesystem(self):
        rbh_sync = find_rbh_sync()
        subprocess.run(
            [
                rbh_sync,
                f"rbh:{self.BACKEND}:{self.test_dir}",
                f"rbh:mongo:{self.mongo_db}"
            ],
            check=True,
        )

    def _write_config(self, config_body):
        content = (
            config_body
            .replace("{fs}", self.test_dir)
            .replace("{db}", self.mongo_db)
        )
        tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False)
        tmp.write(content)
        tmp.close()
        self.config_file_path = tmp.name
        return tmp.name

    def _run_policy(self, config_path, policy_name):
        rbh_policy_script = os.path.join(
            os.path.dirname(os.path.dirname(__file__)),
            "rbh-policy.py"
        )
        return subprocess.run(
            [sys.executable, rbh_policy_script, "run", config_path, policy_name],
            capture_output=True,
            text=True,
        )
