# This file is part of RobinHood 4
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

# Ensure all subprocesses can find backend plugins (mongo, posix, lustre, etc.)
# This mirrors what Meson sets up in librobinhood/src/plugins/meson.build:
# LD_LIBRARY_PATH includes all plugin dirs including posix/enrichers/lustre/
_plugin_dirs = [
    build_lib_path,
    os.path.join(build_lib_path, "plugins", "mongo"),
    os.path.join(build_lib_path, "plugins", "posix"),
    os.path.join(build_lib_path, "plugins", "posix", "enrichers", "lustre"),
    os.path.join(build_lib_path, "plugins", "posix", "enrichers", "retention"),
]
_ld = os.environ.get("LD_LIBRARY_PATH", "")
os.environ["LD_LIBRARY_PATH"] = ":".join(
        filter(None, _plugin_dirs + [_ld]))

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
    action = action.log,
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
    action = action.delete,
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
    action = action.delete,
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
    action = action.log,
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
    action = action.delete,
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
    action = action.delete,
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
    action = action.delete,
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

    def test_cmd_action_success(self):
        """
        Verify that the cmd action executes the specified command on all
        matching files and that parameters are correctly passed to the command.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *
config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)
declare_policy(
    name = "test_cmd_policy",
    target = (Type == "f"),
    action = cmd("ls {exemple_param} {}"),
    parameters = {
        "exemple_param": "-la"
    },
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_cmd_policy")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        output = result.stdout
        for name in ("file1.txt", "file2.log", "file3.csv"):
            file_path = os.path.join(self.test_dir, "filedir", name)
            file_size = os.path.getsize(file_path)

            expected_path = os.path.join(self.test_dir, "filedir", name)

            expected_line = (
                fr"-rw-r--r-- \d+ root root {file_size} "
                fr"\w+\s+\d+\s+\d+:\d+ {expected_path}"
            )

            self.assertRegex(output, expected_line,
                             f"Expected line for {name} not found in output: "
                             f"{expected_line}")

    def test_cmd_action_without_parameters(self):
        """
        Verify that the cmd action works correctly when no additional
        parameters are provided.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *
config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)
declare_policy(
    name = "test_cmd_policy_no_params",
    target = (Type == "f"),
    action = cmd("ls {}"),
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_cmd_policy_no_params")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        output = result.stdout
        for name in ("file1.txt", "file2.log", "file3.csv"):
            self.assertIn(name, output,
                          f"Command output should contain information about "
                          f"{name}")

    def test_cmd_action_with_rules(self):
        """
        Verify that the cmd action works correctly when used with rules.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *
config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)
declare_policy(
    name = "test_cmd_policy_with_rules",
    target = (Type == "f") | (Type == "d"),
    action = action.log,
    trigger = 'Periodic("10m")',
    rules = [
        Rule(
            name = "cmd_rule",
            condition = (Type == "f"),
            action = cmd("ls {exemple_param} {}"),
            parameters = {
                "exemple_param": "-la"
            },
        )
    ]
)
""")
        result = self._run_policy(config_path, "test_cmd_policy_with_rules")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        output = result.stdout

        for name in ("file1.txt", "file2.log", "file3.csv"):
            file_path = os.path.join(self.test_dir, "filedir", name)
            file_size = os.path.getsize(file_path)
            expected_path = os.path.join(self.test_dir, "filedir", name)

            file_pattern = (
                fr"-rw-r--r-- \d+ root root {file_size} "
                fr"\w+\s+\d+\s+\d+:\d+ {expected_path}"
            )
            self.assertRegex(output, file_pattern,
                             f"Expected file line for {name} not found in "
                             f"output")

        for dir_name in ("filedir", "test1", "test2"):
            dir_path = os.path.join(self.test_dir, "filedir", dir_name)

            dir_pattern = (
                fr"drwxr-xr-x \d+ root root "
                fr"\w+ \d+ \d+:\d+ {dir_path}"
            )
            self.assertNotRegex(output, dir_pattern,
                                f"Expected directory line for {dir_name} not "
                                f"found in output")

    def test_cmd_action_command_errors(self):
        """
        Verify that the cmd action correctly handles command execution errors.
        Should test both error handling and proper error reporting.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *
config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)
declare_policy(
    name = "test_error_handling",
    target = (Type == "f"),
    action = cmd("nonexistent_command {}"),
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_error_handling")

        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        output = result.stderr

        for name in ("file1.txt", "file2.log", "file3.csv"):
            file_path = os.path.join(self.test_dir, "filedir", name)
            error_pattern = (
                fr"CmdAction \| command failed \(rc=1\) for '{file_path}': "
                fr"nonexistent_command '{{}}'"
            )
            self.assertRegex(output, error_pattern,
                             f"Expected error message for {name} not found "
                             f"in output")

        for name in ("file1.txt", "file2.log", "file3.csv"):
            self.assertIn(name, output,
                          f"File {name} should have been processed despite "
                          f"the error")

    def test_python_action_success(self):
        """
        Verify that a Python action defined in the config file is called
        with the absolute path and receives parameters as keyword arguments.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

def my_python_action(path, tag=None):
    print(f"PythonAction | path={path} tag={tag}")
    return 0

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_py_action",
    target = (Type == "f") & (Name == "file1.txt"),
    action = my_python_action,
    parameters = {"tag": "test_value"},
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_py_action")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        expected_path = os.path.join(self.test_dir, "filedir", "file1.txt")
        self.assertIn(f"PythonAction | path={expected_path} tag=test_value",
                      result.stdout)

    def test_python_action_from_module(self):
        """
        Verify that a Python action imported from an external module
        is correctly called.
        """
        # Create a Python module in a temporary directory
        config_dir = (
            os.path.dirname(self.config_file_path)
            if self.config_file_path
            else tempfile.gettempdir()
        )
        module_path = os.path.join(config_dir, "test_actions_module.py")
        with open(module_path, "w") as f:
            f.write("""
def external_action(path, label=None):
    print(f"ExternalAction | path={path} label={label}")
    return 0
""")

        config_path = self._write_config("""
from rbhpolicy.config.core import *
import sys
import os

# Add the config directory to sys.path to import our test module
config_dir = os.path.dirname(__file__)
if config_dir not in sys.path:
    sys.path.insert(0, config_dir)

import test_actions_module

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_py_module",
    target = (Type == "f") & (Name == "file2.log"),
    action = test_actions_module.external_action,
    parameters = {"label": "module_test"},
    trigger = 'Periodic("10m")'
)
""")

        try:
            result = self._run_policy(config_path, "test_py_module")
            self.assertEqual(result.returncode, 0,
                             f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                             f"stderr:\n{result.stderr}")

            expected_path = os.path.join(self.test_dir, "filedir", "file2.log")
            self.assertIn(
                f"ExternalAction | path={expected_path} label=module_test",
                result.stdout
            )
        finally:
            if os.path.exists(module_path):
                os.remove(module_path)

    def test_python_action_missing_function(self):
        """
        Verify that referencing a non-existent Python function in the action
        causes an error.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_missing_func",
    target = (Type == "f") & (Name == "file1.txt"),
    action = nonexistent_function,
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_missing_func")
        self.assertNotEqual(result.returncode, 0,
                            "rbh-policy.py should fail when action function "
                            "does not exist")

    def test_python_action_with_exception(self):
        """
        Verify that a Python action that raises an exception is properly
        handled and reported.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

def failing_action(path):
    raise RuntimeError(f"Intentional error for {path}")

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_exception",
    target = (Type == "f") & (Name == "file1.txt"),
    action = failing_action,
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_exception")

        # The policy should still return 0 (process continues despite errors)
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        # Check that the error was reported
        self.assertIn("RuntimeError", result.stderr)
        self.assertIn("Intentional error", result.stderr)

    def test_python_action_without_parameters(self):
        """
        Verify that a Python action works correctly when no parameters
        are provided (only the path argument).
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

def simple_action(path):
    print(f"SimpleAction | {path}")
    return 0

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_simple_py",
    target = (Type == "f") & (Name == "file3.csv"),
    action = simple_action,
    trigger = 'Periodic("10m")'
)
""")
        result = self._run_policy(config_path, "test_simple_py")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        expected_path = os.path.join(self.test_dir, "filedir", "file3.csv")
        self.assertIn(f"SimpleAction | {expected_path}", result.stdout)

    def test_python_action_with_rules(self):
        """
        Verify that Python actions work correctly when used in policy rules.
        """
        config_path = self._write_config("""
from rbhpolicy.config.core import *

def rule_action(path, rule_param=None):
    print(f"RuleAction | path={path} param={rule_param}")
    return 0

config(
    filesystem = "rbh:posix:{fs}",
    database = "rbh:mongo:{db}",
    evaluation_interval = "5s"
)

declare_policy(
    name = "test_py_rules",
    target = (Type == "f") | (Type == "d"),
    action = action.log,
    trigger = 'Periodic("10m")',
    rules = [
        Rule(
            name = "python_rule",
            condition = (Type == "f") & (Name == "file1.txt"),
            action = rule_action,
            parameters = {"rule_param": "from_rule"}
        )
    ]
)
""")
        result = self._run_policy(config_path, "test_py_rules")
        self.assertEqual(result.returncode, 0,
                         f"rbh-policy.py failed:\nstdout:\n{result.stdout}\n"
                         f"stderr:\n{result.stderr}")

        expected_path = os.path.join(self.test_dir, "filedir", "file1.txt")
        self.assertIn(f"RuleAction | path={expected_path} param=from_rule",
                      result.stdout)

        # Other files should be logged (default action)
        self.assertIn("LogAction | path=filedir/file2.log", result.stdout)

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
            os.path.abspath(__file__))))
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

if __name__ == "__main__":
    unittest.main(verbosity=2)
