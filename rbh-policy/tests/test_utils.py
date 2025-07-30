#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import unittest
from datetime import datetime
from rbhpolicy.config.utils import parse_unit

class TestParseUnit(unittest.TestCase):

    def test_storage_units_valid(self):
        self.assertEqual(parse_unit("10B", filter_key="Size"),
                ("10c", "storage", None))
        self.assertEqual(parse_unit("1KB", filter_key="Size"),
                ("1k", "storage", None))
        self.assertEqual(parse_unit("10MB", filter_key="Size"),
                ("10M", "storage", None))
        self.assertEqual(parse_unit("5GB", filter_key="Size"),
                ("5G", "storage", None))
        self.assertEqual(parse_unit("2TB", filter_key="Size"),
                ("2T", "storage", None))
        self.assertEqual(parse_unit("1PB", filter_key="Size"),
                ("1P", "storage", None))
        self.assertEqual(parse_unit("1EB", filter_key="Size"),
                ("1E", "storage",
                         None))
        self.assertEqual(parse_unit("0MB", filter_key="Size"),
                ("0M", "storage", None))

    def test_storage_units_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("100MEG", filter_key="Size")
        with self.assertRaises(ValueError):
            parse_unit("MB10", filter_key="Size")
        with self.assertRaises(ValueError):
            parse_unit("10 MB", filter_key="Size")
        with self.assertRaises(ValueError):
            parse_unit("MB", filter_key="Size")

    def test_quantity_units_valid(self):
        self.assertEqual(parse_unit("100k"), ("100000", "quantity", None))
        self.assertEqual(parse_unit("1M"), ("1000000", "quantity", None))
        self.assertEqual(parse_unit("2B"), ("2000000000", "quantity", None))
        self.assertEqual(parse_unit("1T"), ("1000000000000", "quantity", None))

    def test_quantity_units_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("10ki")
        with self.assertRaises(ValueError):
            parse_unit("B n")

    def test_time_units_valid(self):
        self.assertEqual(parse_unit("30m"), ("30", "time", "m"))
        self.assertEqual(parse_unit("2h"), ("120", "time", "m"))
        self.assertEqual(parse_unit("2d"), ("2", "time", "d"))
        self.assertEqual(parse_unit("1w"), ("7", "time", "d"))
        self.assertEqual(parse_unit("1mo"), ("30", "time", "d"))
        self.assertEqual(parse_unit("1y"), ("365", "time", "d"))
        self.assertEqual(parse_unit("1hour"), ("60", "time", "m"))
        self.assertEqual(parse_unit("2minute"), ("2", "time", "m"))
        self.assertEqual(parse_unit("3day"), ("3", "time", "d"))
        self.assertEqual(parse_unit("2 days"), ("2", "time", "d"))
        self.assertEqual(parse_unit("20 minutes"), ("20", "time", "m"))

    def test_time_units_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("120min")
        with self.assertRaises(ValueError):
            parse_unit("1mon")

    def test_file_type_valid(self):
        self.assertEqual(parse_unit("file"), ("f", "file_type", None))
        self.assertEqual(parse_unit("dir"), ("d", "file_type", None))
        self.assertEqual(parse_unit("symlink"), ("l", "file_type", None))
        self.assertEqual(parse_unit("block"), ("b", "file_type", None))
        self.assertEqual(parse_unit("char"), ("c", "file_type", None))
        self.assertEqual(parse_unit("fifo"), ("p", "file_type", None))
        self.assertEqual(parse_unit("socket"), ("s", "file_type", None))

    def test_file_type_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("folder")
        with self.assertRaises(ValueError):
            parse_unit("")

    def test_integer_valid(self):
        self.assertEqual(parse_unit("0"), ("0", "int", None))
        self.assertEqual(parse_unit("42"), ("42", "int", None))

    def test_integer_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("12.5")
        with self.assertRaises(ValueError):
            parse_unit("Mi")

    def test_date_valid(self):
        self.assertEqual(parse_unit("2024-07-30"),
                         (datetime(2024, 7, 30).isoformat(), "date", None))
        self.assertEqual(parse_unit("30/07/2024"),
                         (datetime(2024, 7, 30).isoformat(), "date", None))
        self.assertEqual(parse_unit("30-07-2024"),
                         (datetime(2024, 7, 30).isoformat(), "date", None))
        self.assertEqual(parse_unit("2024/07/30"),
                         (datetime(2024, 7, 30).isoformat(), "date", None))
        self.assertEqual(parse_unit("2024.07.30"),
                         (datetime(2024, 7, 30).isoformat(), "date", None))
        self.assertEqual(parse_unit("30.07.2024"),
                         (datetime(2024, 7, 30).isoformat(), "date", None))
        self.assertEqual(parse_unit("2024-07-30T13:45:00"),
                         (datetime(2024, 7, 30, 13, 45, 0).isoformat(), "date",
                             None))

    def test_date_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("2025-03-99")
        with self.assertRaises(ValueError):
            parse_unit("31/31/2025")

    def test_invalid_completely(self):
        with self.assertRaises(ValueError):
            parse_unit("abc")
        with self.assertRaises(ValueError):
            parse_unit("!@#")

if __name__ == "__main__":
    unittest.main(verbosity=2)
