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


class TestLustreActionsIntegration(unittest.TestCase):
    """Integration tests for rbh-policy actions on Lustre backend."""

    @classmethod
    def setUpClass(cls):
        """
        Check if Lustre is mounted and backend is available, skip tests if not.
        """
        cls.lustre_path = "/mnt/lustre"
        if not os.path.exists(cls.lustre_path) or (not
                os.path.ismount(cls.lustre_path)):
            raise unittest.SkipTest(
                    f"Lustre filesystem not mounted at {cls.lustre_path}")

        build_lib_path = os.environ.get("BUILD_LIB_PATH")
        if not build_lib_path:
            raise unittest.SkipTest(
                    "BUILD_LIB_PATH environment variable not set")

        lustre_plugin = os.path.join(build_lib_path, "plugins", "posix",
                                     "enrichers", "lustre",
                                     "librbh-posix-lustre-ext.so")
        if not os.path.exists(lustre_plugin):
            raise unittest.SkipTest(
                    f"Lustre enricher plugin not found at {lustre_plugin}")

        project_root = os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.dirname(os.path.abspath(__file__)))))
        cls.rbh_config_path = os.path.join(project_root, "utils", "tests",
                                           "test_config.yaml")
        if not os.path.exists(cls.rbh_config_path):
            raise unittest.SkipTest(
                f"Reference config not found at {cls.rbh_config_path}")

    def setUp(self):
        self.test_dir = os.path.join(self.lustre_path,
                                     f"rbh_test_{os.getpid()}")
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir, ignore_errors=True)
        os.makedirs(self.test_dir, exist_ok=True)

        self.mongo_db = f"test_lustre_{os.getpid()}"
        self.config_file_path = None
        os.environ["RBH_CONFIG_PATH"] = self.rbh_config_path

        try:
            self._create_filesystem_structure()
            self._sync_filesystem()
        except Exception:
            self.tearDown()
            raise

    def tearDown(self):
        if self.config_file_path and os.path.exists(self.config_file_path):
            os.remove(self.config_file_path)
        os.environ.pop("RBH_CONFIG_PATH", None)
        if os.path.exists(self.test_dir):
            shutil.rmtree(self.test_dir, ignore_errors=True)

    def _create_filesystem_structure(self):
        """Create test files in Lustre filesystem."""
        base = os.path.join(self.test_dir, "filedir")
        test1 = os.path.join(base, "test1")
        test2 = os.path.join(test1, "test2")
        os.makedirs(test2, exist_ok=True)

        files_to_create = [
            ("file1.txt", "This is a text file on Lustre.\n"),
            ("file2.log", "Lustre test logs ...\n"),
            ("file3.csv", "col1,col2,col3\n1,2,3\n"),
            ("file4.bin", b"\x00\x01\x02\x03"),
            ("file5.json", '{"key": "lustre_value"}\n'),
        ]

        locations = [
            base,   # file1.txt
            base,   # file2.log
            base,   # file3.csv
            test1,  # file4.bin
            test2,  # file5.json
        ]

        for (name, content), directory in zip(files_to_create, locations):
            path = os.path.join(directory, name)
            mode = "wb" if isinstance(content, bytes) else "w"
            with open(path, mode) as f:
                f.write(content)

    def _sync_filesystem(self):
        """Sync Lustre filesystem to MongoDB backend."""
        rbh_sync = find_rbh_sync()
        subprocess.run(
            [
                rbh_sync,
                f"rbh:lustre:{self.test_dir}",
                f"rbh:mongo:{self.mongo_db}"
            ],
            check=True,
        )

    def _write_config(self, config_body: str) -> str:
        """Create a temporary policy configuration file for Lustre."""
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
            os.path.dirname(os.path.dirname(os.path.dirname(__file__))),
            "rbh-policy.py")
        if not os.path.exists(rbh_policy_script):
            raise RuntimeError(
                f"rbh-policy.py not found at: {rbh_policy_script}")
        return subprocess.run(
            [sys.executable, rbh_policy_script, "run",
                config_path, policy_name],
            capture_output=True,
            text=True,
        )

    def test_log_action_with_lustre_fid(self):
        """
        Verify that the log action on Lustre backend includes the FID
        (File Identifier) in its output, in addition to the path.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:lustre:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_lustre_log_policy",
    target = (Type == "f"),
    action = action.log,
    trigger = 'Periodic("10m")'
)
""")

        result = self._run_policy(config_path, "test_lustre_log_policy")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        output = result.stdout

        # Verify the Lustre-specific format with FID before path
        # Pattern matches: LogAction | fid=[0x...:0x...:0x...], ...
        lustre_log_pattern = re.compile(
            r'LogAction \| fid=\[0x[0-9a-fA-F]+:0x[0-9a-fA-F]+:0x[0-9a-fA-F]+\], '
            r'path=([^,]+), params='
        )

        log_matches = lustre_log_pattern.findall(output)
        self.assertTrue(log_matches,
                        "Lustre log entries should follow format: "
                        "LogAction | fid=[...], path=..., params=...")

        logged_paths = log_matches

        expected_files = [
            "filedir/file1.txt",
            "filedir/file2.log",
            "filedir/file3.csv",
            "filedir/test1/file4.bin",
            "filedir/test1/test2/file5.json"
        ]

        for expected_path in expected_files:
            self.assertIn(expected_path, logged_paths,
                          f"Expected file {expected_path} not found in log output")

        self.assertEqual(len(logged_paths), 5,
                         f"Expected 5 files logged, found {len(logged_paths)}")

        self.assertNotIn("path=filedir,", output)
        self.assertNotIn("path=filedir/test1,", output)

    def test_delete_file_on_lustre(self):
        """
        Verify that the delete action works correctly on Lustre filesystem.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:lustre:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_lustre_delete",
    target = (Type == "f") & (Name == "file1.txt"),
    action = action.delete,
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_lustre_delete")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        deleted = os.path.join(self.test_dir, "filedir", "file1.txt")
        self.assertFalse(
            os.path.exists(deleted),
            "file1.txt should have been deleted from Lustre")

        # Other files must still exist
        for name in ("file2.log", "file3.csv"):
            self.assertTrue(
                os.path.exists(
                    os.path.join(self.test_dir, "filedir", name)),
                f"{name} should not have been deleted")

    def test_log_action_with_posix_on_lustre_fs(self):
        """
        Verify that when using posix.log explicitly on a Lustre filesystem,
        the POSIX log action is used (no Lustre FID in output).
        """

        config_path = self._write_config("""
from rbhpolicy.config.core import *
config(
    filesystem = "rbh:lustre:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)
declare_policy(
    name = "test_posix_log_on_lustre",
    target = (Type == "f"),
    action = posix.log,
    trigger = 'Periodic("10m")'
)
""")

        result = self._run_policy(config_path, "test_posix_log_on_lustre")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )

        output = result.stdout

        # POSIX log format: no fid=[...], only path=...
        posix_log_pattern = re.compile(
            r'LogAction \| path=([^,]+), params='
        )

        log_matches = posix_log_pattern.findall(output)
        self.assertTrue(
            log_matches,
            "POSIX log entries should follow format: "
            "LogAction | path=..., params=..."
        )

        logged_paths = log_matches
        expected_files = [
            "filedir/file1.txt",
            "filedir/file2.log",
            "filedir/file3.csv",
            "filedir/test1/file4.bin",
            "filedir/test1/test2/file5.json"
        ]

        for expected_path in expected_files:
            self.assertIn(
                expected_path, logged_paths,
                f"Expected file {expected_path} not found in log output"
            )

        self.assertEqual(
            len(logged_paths), 5,
            f"Expected 5 files logged, found {len(logged_paths)}"
        )

        # Ensure NO Lustre FID appears
        self.assertNotRegex(output, r'fid= \[')

if __name__ == "__main__":
    unittest.main(verbosity=2)
