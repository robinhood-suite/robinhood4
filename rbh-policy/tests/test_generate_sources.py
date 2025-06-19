#!/usr/bin/env python3.9

'''
This file is part of RobinHood 4
Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                   alternatives
SPDX-License-Identifer: LGPL-3.0-or-later
'''

import os
import subprocess
import unittest

class GenerateSourcesTest(unittest.TestCase):
    def test_generates_expected_files(self):
        test_dir = os.path.dirname(__file__)
        source_root = os.path.abspath(os.path.join(test_dir, '..', '..',
            'rbh-policy'))
        script = os.path.join(source_root, 'generate_sources.py')

        result = subprocess.run(
            [script],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True
        )

        found_files = set()
        for line in result.stdout.strip().splitlines():
            if ':' not in line:
                continue
            _, full_path = line.split(':', 1)
            full_path = full_path.strip()
            rel_path = os.path.relpath(full_path, source_root)
            found_files.add(rel_path)

        expected_files = {
            'rbhpolicy/__init__.py',
            'rbhpolicy/config/__init__.py',
            'rbhpolicy/config/config_loader.py',
        }

        self.assertTrue(expected_files.issubset(found_files),
                        f"Missing from output: {expected_files - found_files}")

if __name__ == '__main__':
    unittest.main()
