#!/usr/bin/env python3.9

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import unittest
import os
import sys

PACKAGE_NAME = 'rbhpolicy'
SCRIPT_PATH = '/usr/bin/rbh-policy'
SITE_PACKAGES_DIR = '/lib/python3.9/site-packages'

class TestRBHPolicySystemInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.package_path = os.path.join(SITE_PACKAGES_DIR, PACKAGE_NAME)
        cls.expected_files = [
            'rbhpolicy/__init__.py',
            'rbhpolicy/config/__init__.py',
            'rbhpolicy/config/config_loader.py',
        ]
        cls.package_installed = os.path.isdir(cls.package_path)

    def test_expected_files_present(self):
        """Verify that expected files are installed"""
        if not self.package_installed:
            self.skipTest("Package not installed — skip file presence test")
        missing = []
        for rel_path in self.expected_files:
            full_path = os.path.join(SITE_PACKAGES_DIR, rel_path)
            if not os.path.isfile(full_path):
                missing.append(full_path)
        self.assertFalse(missing,
                f"Missing installed files:\n" + "\n".join(missing))

    def test_module_importable(self):
        """Check that the module is importable"""
        if not self.package_installed:
            self.skipTest("Package not installed — skip import test")
        try:
            import rbhpolicy.config.config_loader
        except ImportError as e:
            self.fail(f"Failed to import module: {e}")

    def test_script_exists(self):
        """Check that the CLI script is installed and executable"""
        if not os.path.isfile(SCRIPT_PATH):
            self.skipTest("Script not installed — skip executable check")
        self.assertTrue(os.access(SCRIPT_PATH, os.X_OK),
                        f"Script '{SCRIPT_PATH}' is not executable")

if __name__ == '__main__':
    unittest.main(verbosity=2)
