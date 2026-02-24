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

    def test_delete_file_success(self):
        """
        Verify that the delete action removes a targeted file from the
        filesystem, leaving all other files untouched.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_delete_file",
    target = (Type == "f") & (Name == "file1.txt"),
    action = "delete",
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_delete_file")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        deleted = os.path.join(self.test_dir, "filedir", "file1.txt")
        self.assertFalse(
            os.path.exists(deleted),
            "file1.txt should have been deleted")

        # All other files must be untouched
        for name in ("file2.log", "file3.csv"):
            self.assertTrue(
                os.path.exists(
                    os.path.join(self.test_dir, "filedir", name)),
                f"{name} should not have been deleted")

    def test_delete_directory_success(self):
        """
        Verify that the delete action removes an empty directory from the
        filesystem, leaving all other directories untouched.
        """
        empty_dir = os.path.join(self.test_dir, "filedir", "emptydir")
        os.makedirs(empty_dir)
        self._sync_filesystem()

        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_delete_dir",
    target = (Type == "d") & (Name == "emptydir"),
    action = "delete",
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_delete_dir")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        self.assertFalse(
            os.path.exists(empty_dir),
            "emptydir should have been deleted")

        # Other directories must be untouched
        for rel in ("filedir", os.path.join("filedir", "test1")):
            self.assertTrue(
                os.path.exists(os.path.join(self.test_dir, rel)),
                f"{rel} should not have been deleted")

    def test_delete_files_log_dirs_with_rules(self):
        """
        Verify that rules route actions correctly: files matched by a rule
        get deleted, while directories fall back to the default log action.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_mixed_policy",
    target = (Type == "f") | (Type == "d"),
    action = "log",
    trigger = 'Periodic("10m")',
    rules = [
        Rule(
            name = "delete_files_rule",
            condition = (Type == "f"),
            action = "delete",
        )
    ]
)
""")
        result = self._run_policy(config_path, "test_mixed_policy")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        output = result.stdout

        # Files must have been deleted from the filesystem
        for name in ("file1.txt", "file2.log", "file3.csv"):
            self.assertFalse(
                os.path.exists(
                    os.path.join(self.test_dir, "filedir", name)),
                f"{name} should have been deleted by the rule")

        # Directories must still exist (only logged, not deleted)
        for rel in ("filedir",
                    os.path.join("filedir", "test1"),
                    os.path.join("filedir", "test1", "test2")):
            self.assertTrue(
                os.path.exists(os.path.join(self.test_dir, rel)),
                f"{rel} should not have been deleted")

        # Directories must appear in the log output
        self.assertIn("LogAction | path=filedir, params=", output)
        self.assertIn("LogAction | path=filedir/test1, params=", output)

    def test_delete_remove_empty_parent(self):
        """
        Verify that remove_empty_parent causes the parent directory to be
        removed after the last file inside it is deleted.
        """
        solo_dir = os.path.join(self.test_dir, "filedir", "solo_dir")
        os.makedirs(solo_dir)
        solo_file = os.path.join(solo_dir, "solo.txt")
        with open(solo_file, "w") as f:
            f.write("alone\n")
        self._sync_filesystem()

        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_delete_with_parent",
    target = (Type == "f") & (Name == "solo.txt"),
    action = "delete",
    parameters = {"remove_empty_parent": True},
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_delete_with_parent")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        self.assertFalse(
            os.path.exists(solo_file),
            "solo.txt should have been deleted")
        self.assertFalse(
            os.path.exists(solo_dir),
            "solo_dir should have been removed (empty after deletion)")

        # The parent of solo_dir (filedir) must still be present
        self.assertTrue(
            os.path.exists(os.path.join(self.test_dir, "filedir")),
            "filedir should not have been removed")

        self.assertIn("removed empty parent directories", result.stdout)

    def test_delete_remove_parents_below(self):
        """
        Verify that remove_parents_below limits upward parent removal:
        empty parents above the floor path are preserved.
        """
        # Create filedir/a/b/c/ with a single file inside c/
        level_b = os.path.join(self.test_dir, "filedir", "a", "b")
        level_c = os.path.join(level_b, "c")
        os.makedirs(level_c)
        deep_file = os.path.join(level_c, "deep.txt")
        with open(deep_file, "w") as f:
            f.write("deep file\n")
        self._sync_filesystem()

        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_delete_parents_below",
    target = (Type == "f") & (Name == "deep.txt"),
    action = "delete",
    parameters = {
        "remove_empty_parent": True,
        "remove_parents_below": "filedir/a/b",
    },
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_delete_parents_below")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        # The file and its direct empty parent (c/) must be removed
        self.assertFalse(
            os.path.exists(deep_file),
            "deep.txt should have been deleted")
        self.assertFalse(
            os.path.exists(level_c),
            "c/ should have been removed (empty after deletion)")

        # The floor directory (b/) must be preserved
        self.assertTrue(
            os.path.exists(level_b),
            "b/ is the floor and must not have been removed")

    def test_delete_remove_parents_below_without_remove_empty_parent(self):
        """
        Verify that remove_parents_below alone (without remove_empty_parent)
        has no effect: the deleted file's parent directory is preserved.
        """
        alone_dir = os.path.join(self.test_dir, "filedir", "alone_dir")
        os.makedirs(alone_dir)
        alone_file = os.path.join(alone_dir, "alone.txt")
        with open(alone_file, "w") as f:
            f.write("alone\n")
        self._sync_filesystem()

        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_no_remove_parent",
    target = (Type == "f") & (Name == "alone.txt"),
    action = "delete",
    parameters = {"remove_parents_below": "filedir/alone_dir"},
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_no_remove_parent")
        self.assertEqual(
            result.returncode, 0,
            f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}")

        # The file must have been deleted
        self.assertFalse(
            os.path.exists(alone_file),
            "alone.txt should have been deleted")

        # Without remove_empty_parent, the parent must NOT have been removed
        self.assertTrue(
            os.path.exists(alone_dir),
            "alone_dir must be preserved when remove_empty_parent is absent")

        self.assertNotIn("removed empty parent directories", result.stdout)

if __name__ == "__main__":
    unittest.main(verbosity=2)
