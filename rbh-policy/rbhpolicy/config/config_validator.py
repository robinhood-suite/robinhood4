#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.conditions import LogicalCondition
from rbhpolicy.config.policy import Rule

def validate_fileclass(name, condition):
    """
    Validate arguments for declare_fileclass.
    Raise explicit errors if types or values are incorrect.
    """
    if not isinstance(name, str):
        raise TypeError(f"FileClass name must be a string, got "
                        f"{type(name).__name__}")

    if not isinstance(condition, LogicalCondition):
        raise TypeError(f"FileClass condition must be a LogicalCondition"
                        f"instance, got {type(condition).__name__}")

def validate_rule(rule):
    """
    Validate that a Rule instance has correct fields.
    """
    if isinstance(rule, list):
        for r in rule:
            validate_rule(r)
        return

    if not isinstance(rule, Rule):
        raise TypeError(f"Expected a Rule instance, got {type(rule).__name__}")

    if not isinstance(rule.name, str):
        raise TypeError(f"Rule name must be a string, got "
                        f"{type(rule.name).__name__}")

    if not isinstance(rule.condition, LogicalCondition):
        raise TypeError(f"Rule condition must be a LogicalCondition"
                        f"instance, got {type(rule.condition).__name__}")

    if rule.parameters is not None and not isinstance(rule.parameters, dict):
        raise TypeError(f"Rule parameters must be a dict, got "
                        f"{type(rule.parameters).__name__}")

def validate_policy(name, condition, action, trigger, parameters=None,
        rules=None):
    """
    Validate arguments for declare_policy.
    """
    if not isinstance(name, str):
        raise TypeError(f"Policy name must be a string, got "
                        f"{type(name).__name__}")

    if not isinstance(condition, LogicalCondition):
        raise TypeError(f"Policy target must be a LogicalCondition instance, got "
                        f"{type(condition).__name__}")

    if rules is not None:
        if isinstance(rules, Rule):
            validate_rule(rules)
        elif isinstance(rules, (list, tuple)):
            for r in rules:
                validate_rule(r)
        else:
            raise TypeError("rules must be a Rule or a list of Rule")
