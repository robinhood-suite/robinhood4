#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config import filter as rbh

OPERATOR_MAP = {
    "eq": rbh.RBH_FOP_EQUAL,
    "lt": rbh.RBH_FOP_STRICTLY_LOWER,
    "le": rbh.RBH_FOP_LOWER_OR_EQUAL,
    "gt": rbh.RBH_FOP_STRICTLY_GREATER,
    "ge": rbh.RBH_FOP_GREATER_OR_EQUAL,
}

class ConditionBuilder:
    """
    Enables expressions like (Size > 100) by returning SimpleCondition
    instances
    """
    def __init__(self, key: str):
        self.key = key

    def __eq__(self, value):
        return SimpleCondition(self.key, "eq", value)

    def __ne__(self, value):
        return SimpleCondition(self.key, "ne", value)

    def __lt__(self, value):
        return SimpleCondition(self.key, "lt", value)

    def __le__(self, value):
        return SimpleCondition(self.key, "le", value)

    def __gt__(self, value):
        return SimpleCondition(self.key, "gt", value)

    def __ge__(self, value):
        return SimpleCondition(self.key, "ge", value)

class Condition:
    """Abstract base for all conditions."""
    def evaluate(self, data: dict) -> bool:
        raise NotImplementedError("Subclasses must implement evaluate()")

    def __and__(self, other: "Condition") -> "Condition":
        return AndCondition(self, other)

    def __or__(self, other) -> "Condition":
        return OrCondition(self, other)

    def __invert__(self) -> "Condition":
        return NotCondition(self)

    def __repr__(self) -> str:
        return self.__class__.__name__

class SimpleCondition(Condition):
    """Compare a data key against a value with a binary operator."""
    def __init__(self, key: str, operator_name, value):
        self.key = key
        self.operator_name = operator_name
        self.value = value

    def evaluate(self):
        if self.operator_name == "ne":
            eq_filter = rbh.rbh_filter_compare_new(rbh.RBH_FOP_EQUAL, self.key,
                    self.value)
            return rbh.rbh_filter_not(eq_filter)

        op = OPERATOR_MAP.get(self.operator_name)
        if op is None:
            raise ValueError(f"Unsupported operator: {self.operator_name}")
        return rbh.rbh_filter_compare_new(op, self.key, self.value)

    def __repr__(self):
        op_symbol = OP_SYMBOLS.get(self.operator_name, self.operator_name)
        return f"{self.key} {op_symbol} {self.value}"

class AndCondition(Condition):
    """Logical AND of two conditions."""
    def __init__(self, left: Condition, right: Condition):
        self.left = left
        self.right = right

    def evaluate(self):
        return rbh.rbh_filter_and(self.left.evaluate(), self.right.evaluate())

    def __repr__(self):
        return f"({self.left!r} & {self.right!r})"

class OrCondition(Condition):
    """Logical OR of two conditions."""
    def __init__(self, left: Condition, right: Condition):
        self.left = left
        self.right = right

    def evaluate(self):
        return rbh.rbh_filter_or(self.left.evaluate(), self.right.evaluate())

    def __repr__(self):
        return f"({self.left!r} | {self.right!r})"


class NotCondition(Condition):
    """Logical NOT of a condition."""
    def __init__(self, cond: Condition):
        self.cond = cond

    def evaluate(self):
        return rbh.rbh_filter_not(self.cond.evaluate())

    def __repr__(self):
        return f"(~({self.cond!r}))"
