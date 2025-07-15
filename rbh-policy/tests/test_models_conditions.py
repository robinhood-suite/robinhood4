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

build_lib_path = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "../../builddir/librobinhood/src/")
)
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
from rbhpolicy.config.filter import rbh_filter_free, rbh_filter_validate
from rbhpolicy.config.entities import *

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
        cond = (UID == 1000) & (GID == 1000)
        self.assertIsInstance(cond, AndCondition)
        f = cond.to_filter()
        self.assertIsNotNone(f)
        rbh_filter_free(f)

    @verify_with_gdb([
        "op = RBH_FOP_NOT",
        "statx = 8",
        "int64 = 0"
    ], breakpoint_func="rbh_filter_not")
    def test_not_condition_with_gdb(self):
        cond = ~(UID == 0)
        self.assertIsInstance(cond, NotCondition)
        f = cond.to_filter()
        self.assertIsNotNone(f)
        rbh_filter_free(f)

    def test_combined_condition_size_user_or_group(self):
        cond = ((Size > 100) & (User == "usr1")) | (Group == "grp1")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_or_condition_size_user(self):
        cond = (Size > 100) | (User == "usr1")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_not_type_dir(self):
        cond = ~(Type == "dir")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_nested_negation_condition(self):
        cond = ~((UID == 0) | (GID == 0))
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_simple_user_condition(self):
        cond = (User == "alice")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_simple_group_condition(self):
        cond = (Group == "devs")
        f = cond.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

if __name__ == "__main__":
    unittest.main(verbosity=2)
