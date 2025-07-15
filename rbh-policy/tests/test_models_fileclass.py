#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import unittest
from rbhpolicy.config.models.conditions import ConditionBuilder
from rbhpolicy.config.models.fileclass import (
        declare_fileclass,
        rbh_fileclasses,
        FileClass,
)

Size = ConditionBuilder("Size")
User = ConditionBuilder("User")
Group = ConditionBuilder("Group")


class TestFileClass(unittest.TestCase):

    def setUp(self):
        """Clear registry and globals before each test."""
        rbh_fileclasses.clear()
        globals().pop("BigFiles", None)
        globals().pop("AdminsFiles", None)

    @classmethod
    def tearDownClass(cls):
        """Clean up registry and globals after all tests."""
        rbh_fileclasses.clear()
        globals().pop("BigFiles", None)
        globals().pop("AdminsFiles", None)

    def test_declare_fileclass_injects_into_globals(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self.assertIn(
            "BigFiles", globals(),
            msg="declare_fileclass should inject BigFiles into global namespace"
        )

    def test_declare_fileclass_registers_in_rbh_fileclasses(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self.assertIn(
            "BigFiles", rbh_fileclasses,
            msg="declare_fileclass should register BigFiles in rbh_fileclasses"
        )

    def test_declare_fileclass_globals_and_registry_are_same_object(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self.assertIs(
            rbh_fileclasses["BigFiles"], globals()["BigFiles"],
            msg="Globals['BigFiles'] and rbh_fileclasses['BigFiles'] should be "
                "the same object"
        )

    def test_declare_fileclass_creates_fileclass_instance(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self.assertIsInstance(
            BigFiles, FileClass,
            msg="BigFiles should be an instance of FileClass"
        )

    def test_fileclass_evaluates_true_when_condition_matches(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self.assertTrue(
            BigFiles.evaluate({"Size": 1500}),
            msg="BigFiles should match when Size >= 1000 with Size=1500"
        )

    def test_fileclass_evaluates_false_when_condition_does_not_match(self):
        declare_fileclass(name="BigFiles", target=Size >= 1000)
        self.assertFalse(
            BigFiles.evaluate({"Size": 500}),
            msg="BigFiles should not match when Size < 1000 with Size=500"
        )

    def test_fileclass_and_combination_matches_when_both_conditions_true(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        combined_cond = AdminsFiles & (Size > 100)
        self.assertTrue(
            combined_cond.evaluate({"User": "admin1", "Size": 200}),
            msg="AdminsFiles & (Size > 100) should match when both conditions "
                "are true"
        )

    def test_fileclass_and_combination_fails_when_one_condition_false(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        combined_cond = AdminsFiles & (Size > 100)
        self.assertFalse(
            combined_cond.evaluate({"User": "usr2", "Size": 200}),
            msg="AdminsFiles & (Size > 100) should not match when User != "
                "admin1"
        )

    def test_fileclass_and_combination_fails_when_both_conditions_false(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        combined_cond = AdminsFiles & (Size > 100)
        self.assertFalse(
            combined_cond.evaluate({"User": "usr2", "Size": 50}),
            msg="AdminsFiles & (Size > 100) should not match when both "
                "conditions are false"
        )

    def test_fileclass_or_combination_matches_when_first_condition_true(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        or_cond = AdminsFiles | (Group == "grp1")
        self.assertTrue(
            or_cond.evaluate({"User": "admin1"}),
            msg="AdminsFiles | (Group == grp1) should match when User=admin1"
        )

    def test_fileclass_or_combination_matches_when_second_condition_true(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        or_cond = AdminsFiles | (Group == "grp1")
        self.assertTrue(
            or_cond.evaluate({"Group": "grp1"}),
            msg="AdminsFiles | (Group == grp1) should match when Group=grp1"
        )

    def test_fileclass_or_combination_fails_when_both_conditions_false(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        or_cond = AdminsFiles | (Group == "grp1")
        self.assertFalse(
            or_cond.evaluate({"User": "usr2", "Group": "grp2"}),
            msg="AdminsFiles | (Group == grp1) should not match when both "
                "conditions are false"
        )

    def test_fileclass_not_combination_matches_when_condition_false(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        not_cond = ~AdminsFiles
        self.assertTrue(
            not_cond.evaluate({"User": "usr2"}),
            msg="~AdminsFiles should match when User != admin1"
        )

    def test_fileclass_not_combination_fails_when_condition_true(self):
        declare_fileclass(name="AdminsFiles", target=User == "admin1")
        not_cond = ~AdminsFiles
        self.assertFalse(
            not_cond.evaluate({"User": "admin1"}),
            msg="~AdminsFiles should not match when User=admin1"
        )

if __name__ == "__main__":
    unittest.main(verbosity=2)
