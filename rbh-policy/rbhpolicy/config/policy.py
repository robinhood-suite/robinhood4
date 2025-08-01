#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import sys
from rbhpolicy.config.conditions import Condition
from rbhpolicy.config.config_validator import validate_policy

rbh_policies = {}

class Rule:
    """Represents a named rule that may apply within a Policy."""
    def __init__(self, *, name: str, condition: Condition, action=None,
            parameters=None):
        self.name = name
        self.condition = condition
        self.action = action
        self.parameters = parameters or {}
        self._filter = None

    def to_filter(self):
        if self._filter is None:
            self._filter = self.condition.to_filter()
        return self._filter

    def __repr__(self):
        action_name = getattr(self.action, '__name__', str(self.action))
        return (f"Rule<{self.name}>("
                f"condition={self.condition}, action={action_name}, "
                f"parameters={self.parameters})")

class Policy:
    """
    A named policy defined by a target condition, action, trigger and
    optional rules.
    """
    def __init__(self, name: str, target: Condition, action, trigger,
            parameters=None, rules=None):
        self.name = name
        self.target = target
        self.action = action
        self.trigger = trigger
        self.parameters = parameters or {}
        self.rules = rules or []
        self._filter = None

    def to_filter(self):
        if self._filter is None:
            self._filter = self.target.to_filter()
        return self._filter

    def __repr__(self):
        action_name = getattr(self.action, '__name__', str(self.action))
        rules_names = [r.name for r in self.rules]
        return (f"Policy<{self.name}>("
                f"target={self.target}, action={action_name}, "
                f"trigger={self.trigger}, rules={rules_names}, "
                f"parameters={self.parameters})")

def declare_policy(*, name: str, target, action, trigger, parameters=None,
                   rules=None) -> Policy:
    """
    Declare a new policy and inject it into the caller’s namespace
    so it’s directly available in config files.
    """
    validate_policy(name, target, action, trigger, parameters, rules)

    policy = Policy(name, target, action, trigger, parameters, rules)
    policy._filter = target.to_filter()

    if isinstance(policy.rules, Rule):
        policy.rules._filter = policy.rules.condition.to_filter()
    elif isinstance(policy.rules, (list, tuple)):
        for rule in policy.rules:
            rule._filter = rule.condition.to_filter()

    rbh_policies[name] = policy
    caller_ns = sys._getframe(1).f_globals
    caller_ns[name] = policy

    return policy

