#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import unittest
from rbhpolicy.config.models.conditions import (
    SimpleCondition,
    AndCondition,
    OrCondition,
    NotCondition,
    ConditionBuilder,
)

Size = ConditionBuilder("Size")
User = ConditionBuilder("User")
Group = ConditionBuilder("Group")

class TestSimpleCondition(unittest.TestCase):

    def test_eq_condition(self):
        cond = Size == 100
        self.assertIsInstance(cond, SimpleCondition)
        self.assertTrue(
            cond.evaluate({"Size": 100}),
            msg=f"{cond} should match when Size == 100"
        )
        self.assertFalse(
            cond.evaluate({"Size": 50}),
            msg=f"{cond} should not match when Size != 100"
        )

    def test_ne_condition(self):
        cond = User != "usr1"
        self.assertTrue(
            cond.evaluate({"User": "usr2"}),
            msg=f"{cond} should match when User != 'usr1'"
        )
        self.assertFalse(
            cond.evaluate({"User": "usr1"}),
            msg=f"{cond} should not match when User == 'usr1'"
        )

    def test_gt_condition(self):
        cond = Size > 50
        self.assertTrue(
            cond.evaluate({"Size": 60}),
            msg=f"{cond} should match when Size > 50"
        )
        self.assertFalse(
            cond.evaluate({"Size": 40}),
            msg=f"{cond} should not match when Size <= 50"
        )

    def test_ge_condition(self):
        cond = Size >= 100
        self.assertTrue(
            cond.evaluate({"Size": 100}),
            msg=f"{cond} should match when Size == 100"
        )
        self.assertTrue(
            cond.evaluate({"Size": 150}),
            msg=f"{cond} should match when Size > 100"
        )
        self.assertFalse(
            cond.evaluate({"Size": 50}),
            msg=f"{cond} should not match when Size < 100"
        )

    def test_lt_condition(self):
        cond = Size < 200
        self.assertTrue(
            cond.evaluate({"Size": 150}),
            msg=f"{cond} should match when Size < 200"
        )
        self.assertFalse(
            cond.evaluate({"Size": 300}),
            msg=f"{cond} should not match when Size >= 200"
        )

    def test_le_condition(self):
        cond = Size <= 100
        self.assertTrue(
            cond.evaluate({"Size": 50}),
            msg=f"{cond} should match when Size < 100"
        )
        self.assertTrue(
            cond.evaluate({"Size": 100}),
            msg=f"{cond} should match when Size == 100"
        )
        self.assertFalse(
            cond.evaluate({"Size": 150}),
            msg=f"{cond} should not match when Size > 100"
        )

    def test_missing_key(self):
        cond = User == "usr1"
        self.assertFalse(
            cond.evaluate({"Size": 100}),
            msg=f"{cond} should not match when 'User' key is missing"
        )


class TestLogicalConditions(unittest.TestCase):

    def test_and_condition(self):
        cond = (Size > 100) & (User == "usr1")
        self.assertIsInstance(cond, AndCondition)

        self.assertTrue(
            cond.evaluate({"Size": 150, "User": "usr1"}),
            msg=f"{cond} should match when both conditions are True"
        )
        self.assertFalse(
            cond.evaluate({"Size": 90, "User": "usr1"}),
            msg=f"{cond} should not match when Size <= 100"
        )
        self.assertFalse(
            cond.evaluate({"Size": 150, "User": "usr2"}),
            msg=f"{cond} should not match when User != 'usr1'"
        )

    def test_or_condition(self):
        cond = (Size > 100) | (User == "usr1")
        self.assertIsInstance(cond, OrCondition)

        self.assertTrue(
            cond.evaluate({"Size": 150, "User": "usr2"}),
            msg=f"{cond} should match because Size > 100"
        )
        self.assertTrue(
            cond.evaluate({"Size": 50, "User": "usr1"}),
            msg=f"{cond} should match because User == 'usr1'"
        )
        self.assertFalse(
            cond.evaluate({"Size": 50, "User": "usr2"}),
            msg=f"{cond} should not match when both conditions are False"
        )

    def test_not_condition(self):
        cond = ~(User == "usr1")
        self.assertIsInstance(cond, NotCondition)

        self.assertTrue(
            cond.evaluate({"User": "usr2"}),
            msg=f"{cond} should match because User != 'usr1'"
        )
        self.assertFalse(
            cond.evaluate({"User": "usr1"}),
            msg=f"{cond} should not match because User == 'usr1'"
        )

    def test_nested_conditions(self):
        cond = ((Size > 100) & (User == "usr1")) | (Group == "grp1")
        self.assertTrue(
            cond.evaluate({"Size": 50, "User": "usr2", "Group": "grp1"}),
            msg=f"{cond} should match because Group == 'grp1'"
        )
        self.assertTrue(
            cond.evaluate({"Size": 150, "User": "usr1", "Group": "grp2"}),
            msg=f"{cond} should match because Size > 100 and User == 'usr1'"
        )
        self.assertFalse(
            cond.evaluate({"Size": 50, "User": "usr2", "Group": "grp2"}),
            msg=f"{cond} should not match because all conditions failed"
        )

    def test_repr_output(self):
        cond = ((Size > 100) & (User == "usr1")) | ~(Group == "grp1")
        repr_str = repr(cond)
        self.assertIn("AND", repr_str,
                      msg=f"repr should contain 'AND': {repr_str}")
        self.assertIn("OR", repr_str,
                      msg=f"repr should contain 'OR': {repr_str}")
        self.assertIn("NOT", repr_str,
                      msg=f"repr should contain 'NOT': {repr_str}")
        self.assertIn("==", repr_str,
                      msg=f"repr should contain '==': {repr_str}")
        self.assertIn(">", repr_str,
                      msg=f"repr should contain '>': {repr_str}")

if __name__ == "__main__":
    unittest.main(verbosity=2)
