#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import unittest
from datetime import datetime
from rbhpolicy.config.utils import parse_unit

class TestParseUnit(unittest.TestCase):

    def test_storage_units_valid(self):
        self.assertEqual(parse_unit("10MB"), ("10M", "storage", None))
        self.assertEqual(parse_unit("2PB"), ("2048T", "storage", None))

    def test_storage_units_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("100MEG")
        with self.assertRaises(ValueError):
            parse_unit("MB")

    def test_quantity_units_valid(self):
        self.assertEqual(parse_unit("100k"), ("100000", "quantity", None))
        self.assertEqual(parse_unit("1Bn"), ("1000000000", "quantity", None))

    def test_quantity_units_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("10ki")
        with self.assertRaises(ValueError):
            parse_unit("B n")

    def test_time_units_valid(self):
        self.assertEqual(parse_unit("30m"), ("30", "time", "m"))
        self.assertEqual(parse_unit("2d"), ("2", "time", "d"))

    def test_time_units_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("120min")
        with self.assertRaises(ValueError):
            parse_unit("2 h")

    def test_file_type_valid(self):
        self.assertEqual(parse_unit("file"), ("f", "file_type", None))
        self.assertEqual(parse_unit("dir"), ("d", "file_type", None))

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

    def test_date_valid(self):
        self.assertEqual(parse_unit("2024-07-30")[1], "date")
        self.assertEqual(parse_unit("30/07/2024")[1], "date")

    def test_date_invalid(self):
        with self.assertRaises(ValueError):
            parse_unit("2025-03-99")
        with self.assertRaises(ValueError):
            parse_unit("31/31/2025")

if __name__ == "__main__":
    unittest.main(verbosity=2)
