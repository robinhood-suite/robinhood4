#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import operator

OP_SYMBOLS = {
    'eq': '==',
    'ne': '!=',
    'lt': '<',
    'le': '<=',
    'gt': '>',
    'ge': '>='
}

class ConditionBuilder:
    """
    Enables expressions like (Size > 100) by returning SimpleCondition
    instances
    """
    def __init__(self, key: str):
        self.key = key

    def __eq__(self, value):
        return SimpleCondition(self.key, operator.eq, value)

    def __ne__(self, value):
        return SimpleCondition(self.key, operator.ne, value)

    def __lt__(self, value):
        return SimpleCondition(self.key, operator.lt, value)

    def __le__(self, value):
        return SimpleCondition(self.key, operator.le, value)

    def __gt__(self, value):
        return SimpleCondition(self.key, operator.gt, value)

    def __ge__(self, value):
        return SimpleCondition(self.key, operator.ge, value)

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
    def __init__(self, key: str, operator_func, value):
        self.key = key
        self.operator_func = operator_func
        self.value = value

    def evaluate(self, data: dict) -> bool:
        actual = data.get(self.key)
        if actual is None:
            return False
        return self.operator_func(actual, self.value)

    def __repr__(self) -> str:
        op_name = self.operator_func.__name__
        op_symbol = OP_SYMBOLS.get(op_name, op_name)
        return f"{self.key} {op_symbol} {self.value}"

class AndCondition(Condition):
    """Logical AND of two conditions."""
    def __init__(self, left: Condition, right: Condition):
        self.left = left
        self.right = right

    def evaluate(self, data: dict) -> bool:
        return self.left.evaluate(data) and self.right.evaluate(data)

    def __repr__(self) -> str:
        return f"({self.left!r} AND {self.right!r})"


class OrCondition(Condition):
    """Logical OR of two conditions."""
    def __init__(self, left: Condition, right: Condition):
        self.left = left
        self.right = right

    def evaluate(self, data: dict) -> bool:
        return self.left.evaluate(data) or self.right.evaluate(data)

    def __repr__(self) -> str:
        return f"({self.left!r} OR {self.right!r})"


class NotCondition(Condition):
    """Logical NOT of a condition."""
    def __init__(self, cond: Condition):
        self.cond = cond

    def evaluate(self, data: dict) -> bool:
        return not self.cond.evaluate(data)

    def __repr__(self) -> str:
        return f"(NOT {self.cond!r})"
