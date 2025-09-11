#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import subprocess
import unittest
import ctypes
import os

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

from rbhpolicy.config.conditions import (
    SimpleCondition,
    AndCondition,
    OrCondition,
    NotCondition,
    ConditionBuilder,
)
from rbhpolicy.config.filter import (
        rbh_filter_free,
        rbh_filter_validate,
        set_backend,
)
import rbhpolicy.config.entities as entities

set_backend("rbh:lustre:test")

def verify_with_gdb(gdb_expressions, breakpoint_func="rbh_filter_compare_new"):
    def decorator(test_func):
        def wrapper(self, *args, **kwargs):
            test_func(self, *args, **kwargs)

            gdb_script_lines = [
                "set breakpoint pending on",
                f"b {breakpoint_func}",
                "run",
                "finish",
                "print $1",
                "print *$1",
                "print *$1->logical->filters[0]",
                "print *$1->logical->filters[1]"
            ]
            gdb_script_path = "/tmp/gdb_check_script.gdb"
            with open(gdb_script_path, "w") as f:
                f.write("\n".join(gdb_script_lines))

            result = subprocess.run(
                ["gdb", "--batch", "-x", gdb_script_path,
                 "--args", "python3.9", __file__],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True
            )

            output = result.stdout
            for expected in gdb_expressions:
                self.assertIn(expected, output, msg=f"[GDB] '{expected}' not "
                        "found ")

            if os.path.exists(gdb_script_path):
                os.remove(gdb_script_path)

        return wrapper
    return decorator

class TestLogicalConditions(unittest.TestCase):

    @verify_with_gdb([
        "op = RBH_FOP_AND",
        "op = RBH_FOP_EQUAL",
        "statx = 8",  # UID
        "int64 = 1000",
        "statx = 16",  # GID
        "int64 = 1000"
    ], breakpoint_func="rbh_filter_and")
    def test_and_condition_with_gdb(self):
        cond = (entities.UID == 1000) & (entities.GID == 1000)
        self.assertIsInstance(cond, AndCondition)
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    @verify_with_gdb([
        "op = RBH_FOP_NOT",
        "statx = 8",
        "int64 = 0"
    ], breakpoint_func="rbh_filter_not")
    def test_not_condition_with_gdb(self):
        cond = ~(entities.UID == 0)
        self.assertIsInstance(cond, NotCondition)
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_combined_condition_size_user_or_group(self):
        cond =( ((entities.Size > 100) & (entities.User == "root"))
                | (entities.Group == "root"))
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_or_condition_size_user(self):
        cond = (entities.Size > 100) | (entities.User == "root")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_not_type_dir(self):
        cond = ~(entities.Type == "dir")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_nested_negation_condition(self):
        cond = ~((entities.UID == 0) | (entities.GID == 0))
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_simple_user_condition(self):
        cond = (entities.User == "root")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_simple_group_condition(self):
        cond = (entities.Group == "root")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

class TestInvalidConditions(unittest.TestCase):
    def test_invalid_uid_string(self):
        cond = (entities.UID == "usr1")
        with self.assertRaises((TypeError, ValueError)):
            cond.to_filter()

    def test_invalid_gid_string(self):
        cond = (entities.GID == "abc")
        with self.assertRaises((TypeError, ValueError)):
            cond.to_filter()

    def test_invalid_size_string(self):
        cond = (entities.Size > "Patrick")
        with self.assertRaises((TypeError, ValueError)):
            cond.to_filter()

    def test_unknown_field(self):
        Unknown = entities.ConditionBuilder("UnknownField")
        cond = (Unknown == "test")
        with self.assertRaises(ValueError) as cm:
            cond.to_filter()
        self.assertIn("Unknown key", str(cm.exception))

    def test_invalid_operator_for_size(self):
        cond = entities.Size != 100
        with self.assertRaises(ValueError) as cm:
            cond.to_filter()
        self.assertIn("Unsupported operator for Size", str(cm.exception))

    def test_invalid_time_kind(self):
        cond = (entities.LastAccess > "nonsense")
        with self.assertRaises((TypeError, ValueError)):
            cond.to_filter()

if __name__ == "__main__":
    unittest.main(verbosity=2)
