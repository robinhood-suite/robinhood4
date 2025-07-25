#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import unittest
from rbhpolicy.config.policy import Policy, Rule, declare_policy, \
        rbh_policies
from rbhpolicy.config.conditions import ConditionBuilder

Size = ConditionBuilder("Size")
User = ConditionBuilder("User")

def action():
    return "action executed"

def Scheduled(str):
    return True


class TestRuleAndPolicy(unittest.TestCase):
    def setUp(self):
        rbh_policies.clear()

    def test_rule_creation_with_valid_values(self):
        rule = Rule(
            name="MyRule",
            condition=User == "usr1",
            action=action,
            parameters={"key": "value"}
        )
        self.assertEqual(rule.name, "MyRule",
                msg="Rule name should be set correctly")
        self.assertIsNotNone(rule.condition,
                msg="Rule condition should not be None")
        self.assertIs(rule.action, action,
                msg="Rule action should be stored correctly")
        self.assertEqual(rule.parameters, {"key": "value"},
                msg="Rule parameters should be stored correctly")
    def test_policy_creation_with_valid_values(self):
        rules = [
            Rule(
                name="R1",
                condition=Size < "200",
                action=action
            ),
            Rule(
                name="R2",
                condition=Size >= "200",
                action=action
            )
        ]
        policy = Policy(
            name="MyPolicy",
            target=Size > "100",
            action=action,
            trigger=Scheduled("2025-06-01 03:00"),
            parameters={"threshold": 10},
            rules=rules
        )

        self.assertEqual(policy.name, "MyPolicy",
                msg="Policy name should be set correctly")
        self.assertIsNotNone(policy.target,
                msg="Policy target should not be None")
        self.assertIs(policy.action, action,
                msg="Policy action should be stored correctly")
        self.assertEqual(policy.trigger, Scheduled("2025-06-01 03:00"),
                msg="Policy trigger should be stored correctly")
        self.assertEqual(policy.parameters, {"threshold": 10},
                msg="Policy parameters should be stored correctly")
        self.assertEqual(policy.rules, rules,
                msg="Policy rules should be stored correctly")

    def test_repr_methods_do_not_crash(self):
        rule = Rule(
            name="R",
            condition=Size < "500",
            action=action
        )
        policy = Policy(
            name="P",
            target=User == "test_user",
            action=action,
            trigger=Scheduled("2025-06-01 03:00")
        )
        self.assertIsInstance(repr(rule), str,
                msg="Rule __repr__ should return a string")
        self.assertIsInstance(repr(policy), str,
                msg="Policy __repr__ should return a string")

    def test_declare_policy_registers_and_injects(self):
        policy = declare_policy(
            name="AutoPolicy",
            target=User == "admin",
            action=action,
            trigger=Scheduled("2025-06-01 03:00")
        )
        self.assertIn("AutoPolicy", rbh_policies,
                msg="Policy should be registered in rbh_policies")
        self.assertIs(rbh_policies["AutoPolicy"], policy,
                msg="Registered policy should match returned instance")
        self.assertIs(globals().get("AutoPolicy"), policy,
                msg="Policy should be injected into caller namespace")


if __name__ == "__main__":
    unittest.main(verbosity=2)
