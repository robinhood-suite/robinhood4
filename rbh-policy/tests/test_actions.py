# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import unittest
import ctypes
import os
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

# Ensure all subprocesses can find backend plugins (mongo, posix, etc.)
_mongo_plugin_path = os.path.join(build_lib_path, "plugins", "mongo")
_ld = os.environ.get("LD_LIBRARY_PATH", "")
os.environ["LD_LIBRARY_PATH"] = ":".join(
        filter(None, [_mongo_plugin_path, _ld]))

ctypes.CDLL = _patched_cdll

def get_build_root():
    return os.path.dirname(os.path.dirname(build_lib_path))

def find_rbh_sync():
    build_root = get_build_root()
    path = os.path.join(build_root, "rbh-sync", "rbh-sync")
    if not os.path.exists(path):
        raise RuntimeError(f"rbh-sync not found at expected location: {path}")
    return path

class TestActionsIntegration(unittest.TestCase):
    """Integration tests for rbh-policy actions."""
    def setUp(self):
        self.test_dir = tempfile.mkdtemp(prefix="rbh_test_")
        self.mongo_db = f"test_{os.getpid()}"
        self.config_file_path = None

        self._create_filesystem_structure()
        self._sync_filesystem()

    def tearDown(self):
        if self.config_file_path and os.path.exists(self.config_file_path):
            os.remove(self.config_file_path)
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def _create_filesystem_structure(self):
        base = os.path.join(self.test_dir, "filedir")
        test1 = os.path.join(base, "test1")
        test2 = os.path.join(test1, "test2")
        os.makedirs(test2)

        files_to_create = [
            ("file1.txt", "This is a text file.\n"),
            ("file2.log", "Test logs ...\n"),
            ("file3.csv", "col1,col2,col3\n1,2,3\n"),
            ("file4.bin", b"\x00\x01\x02\x03"),
            ("file5.json", '{"key": "value"}\n'),
            ("file6.xml", "<root><item>test</item></root>\n"),
            ("file7.sh", "#!/bin/bash\necho Hello\n"),
            ("file8.py", "print('Hello RBH')\n"),
            ("file9.dat", "blablablabla\n"),
            ("file10.big", "X" * 1_000_000),
        ]

        locations = [
            base,   # file1.txt
            base,   # file2.log
            base,   # file3.csv
            test1,  # file4.bin
            test1,  # file5.json
            test1,  # file6.xml
            test2,  # file7.sh
            test2,  # file8.py
            test2,  # file9.dat
            test2,  # file10.big
        ]

        for (name, content), directory in zip(files_to_create, locations):
            path = os.path.join(directory, name)
            mode = "wb" if isinstance(content, bytes) else "w"
            with open(path, mode) as f:
                f.write(content)

    def _sync_filesystem(self):
        rbh_sync = find_rbh_sync()
        subprocess.run(
            [
                rbh_sync,
                f"rbh:posix:{self.test_dir}",
                f"rbh:mongo:{self.mongo_db}"
            ],
            check=True,
        )

    def _write_config(self, config_body: str) -> str:
        """Create a temporary policy configuration file."""
        content = (config_body
                   .replace("{fs}", self.test_dir)
                   .replace("{db}", self.mongo_db))
        tmp_file = tempfile.NamedTemporaryFile(mode="w", suffix=".py",
                                               delete=False)
        tmp_file.write(content)
        tmp_file.close()
        self.config_file_path = tmp_file.name
        return tmp_file.name

    def _run_policy(self, config_path: str, policy_name: str):
        """Run a policy via rbh-policy.py and return CompletedProcess."""
        rbh_policy_script = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "rbh-policy.py")
        if not os.path.exists(rbh_policy_script):
            raise RuntimeError(
                f"rbh-policy.py not found at: {rbh_policy_script}")
        return subprocess.run(
            [sys.executable, rbh_policy_script, "run",
                config_path, policy_name],
            capture_output=True,
            text=True,
        )

    def test_log_action_success(self):
        """
        Verify that the log action is applied to all files (not dirs)
        and that output paths are relative (no absolute path leak).
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_log_policy",
    target = (Type == "f"),
    action = "log",
    trigger = 'Periodic("10m")'
)
""")

        result = self._run_policy(config_path, "test_log_policy")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        output = result.stdout

        # Files must appear in output
        self.assertIn("LogAction | path=filedir/file1.txt", output)
        self.assertIn(
            "LogAction | path=filedir/test1/test2/file10.big", output
        )
        self.assertIn("params=", output)

        # Directories must not be logged
        self.assertNotIn("LogAction | path=filedir,", output)
        self.assertNotIn("LogAction | path=filedir/test1,", output)
        self.assertNotIn("LogAction | path=filedir/test1/test2,", output)

        # Absolute paths must not leak into output
        self.assertNotIn(self.test_dir, output)

    def test_log_action_missing_action_field(self):
        """
        Verify that omitting the 'action' field in declare_policy
        causes rbh-policy.py to fail with a non-zero exit code.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_log_policy",
    target = (Type == "f"),
    trigger = 'Periodic("10m")'
)
""")

        result = self._run_policy(config_path, "test_log_policy")
        self.assertNotEqual(result.returncode, 0,
                            "rbh-policy.py should have failed when 'action' "
                            "is missing from declare_policy")

if __name__ == "__main__":
    unittest.main(verbosity=2)
