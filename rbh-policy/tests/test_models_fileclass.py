#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
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

from rbhpolicy.config.fileclass import (
        declare_fileclass,
        rbh_fileclasses,
        FileClass,
)
from rbhpolicy.config.filter import rbh_filter_validate, rbh_filter_free
from rbhpolicy.config.entities import *

class TestFileClassBehavior(unittest.TestCase):
    def setUp(self):
        rbh_fileclasses.clear()
        globals().pop("BigFiles", None)
        globals().pop("AdminsFiles", None)

    @classmethod
    def tearDownClass(cls):
        rbh_fileclasses.clear()
        globals().pop("BigFiles", None)
        globals().pop("AdminsFiles", None)

    def _validate_class(self, fc):
        f = fc.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_declare_and_validate_fileclass(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self._validate_class(BigFiles)

    def test_combined_fileclass_and_condition(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        combined = AdminsFiles & (Size > 100)
        f = combined.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_combined_fileclass_or_condition(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        combined = AdminsFiles | (Group == "grp1")
        f = combined.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

    def test_negated_fileclass(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        negated = ~AdminsFiles
        f = negated.to_filter()
        self.assertIsNotNone(f)
        self.assertEqual(rbh_filter_validate(f), 0)
        rbh_filter_free(f)

class TestFileClassDeclarationErrors(unittest.TestCase):
    def tearDown(self):
        rbh_fileclasses.clear()
        globals().pop("FaultyClass", None)

    def test_missing_name_argument(self):
        with self.assertRaises(TypeError):
            declare_fileclass(target=Size > 100)

    def test_missing_target_argument(self):
        with self.assertRaises(TypeError):
            declare_fileclass(name="FaultyClass")

if __name__ == "__main__":
    unittest.main(verbosity=2)
