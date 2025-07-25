# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import unittest
import ctypes
import os
import sys

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

from rbhpolicy.config.filter import (
    rbh_filter_compare_new,
    rbh_filter_and,
    rbh_filter_or,
    rbh_filter_not,
    rbh_filter_validate,
    rbh_filter_free,
    RBH_FOP_EQUAL,
    RBH_FOP_STRICTLY_LOWER,
    RBH_FOP_STRICTLY_GREATER,
)

class TestFilter(unittest.TestCase):
    def test_string_field_filter(self):
        """Test filter on a string field: Type == 'file'"""
        print("[TEST] Creating filter: Type == 'file'")
        f = rbh_filter_compare_new(RBH_FOP_EQUAL, "Type", "f")
        self.assertIsNotNone(f,
                "[ERROR] rbh_filter_compare_new returned NULL for Type == 'file'"
        )
        print("[TEST] Validating filter...")
        self.assertEqual(rbh_filter_validate(f), 0)
        print("[OK] Filter created successfully for Type == 'file'")
        rbh_filter_free(f)

    def test_logical_and_different_fields(self):
        """Test combining filters on different fields with AND"""
        print("[TEST] Combining filters: UID == 1000 & GID == 1000")
        f1 = rbh_filter_compare_new(RBH_FOP_EQUAL, "UID", 1000)
        f2 = rbh_filter_compare_new(RBH_FOP_EQUAL, "GID", 1000)
        combined = rbh_filter_and(f1, f2)
        self.assertIsNotNone(combined,
                "[ERROR] rbh_filter_and returned NULL for UID & GID"
        )
        print("[TEST] Validating filter...")
        self.assertEqual(rbh_filter_validate(f1), 0)
        self.assertEqual(rbh_filter_validate(f2), 0)
        self.assertEqual(rbh_filter_validate(combined), 0)
        print("[OK] Combined filter created successfully for UID & GID")
        rbh_filter_free(combined)

    def test_logical_or_different_fields(self):
        """Test combining filters on different fields with OR"""
        print("[TEST] Combining filters: Type == 'file' | Type == 'dir'")
        f1 = rbh_filter_compare_new(RBH_FOP_EQUAL, "Type", "f")
        f2 = rbh_filter_compare_new(RBH_FOP_EQUAL, "Type", "d")
        combined = rbh_filter_or(f1, f2)
        self.assertIsNotNone(combined,
                "[ERROR] rbh_filter_or returned NULL for Type == 'file' | 'dir'"
        )
        print("[TEST] Validating filter...")
        self.assertEqual(rbh_filter_validate(f1), 0)
        self.assertEqual(rbh_filter_validate(f2), 0)
        self.assertEqual(rbh_filter_validate(combined), 0)
        print("[OK] Combined filter created successfully for Type == file | dir")
        rbh_filter_free(combined)

    def test_negate_filter_on_uid(self):
        """Test negating a filter: ~(UID == 0)"""
        print("[TEST] Negating filter: ~(UID == 0)")
        f = rbh_filter_compare_new(RBH_FOP_EQUAL, "UID", 0)
        negated = rbh_filter_not(f)
        self.assertIsNotNone(negated,
                "[ERROR] rbh_filter_not returned NULL for ~(UID == 0)"
        )
        print("[TEST] Validating filter...")
        self.assertEqual(rbh_filter_validate(f), 0)
        print("[OK] Negated filter created successfully for ~(UID == 0)")
        rbh_filter_free(negated)

if __name__ == "__main__":
    unittest.main(verbosity=2)
