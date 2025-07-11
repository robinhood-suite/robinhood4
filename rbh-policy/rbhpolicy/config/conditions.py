#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from abc import ABC, abstractmethod
from typing import TypeVar
from rbhpolicy.config import filter as rbh

T = TypeVar("T", bound="Condition")

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

class Condition(ABC):
    """Abstract base for all conditions."""
    @abstractmethod
    def to_filter(self):
        pass

    def __and__(self: T, other: T) -> T:
        return AndCondition(self, other)

    def __or__(self: T, other: T) -> T:
        return OrCondition(self, other)

    def __invert__(self: T) -> T:
        return NotCondition(self)

    def __repr__(self) -> str:
        return self.__class__.__name__

class SimpleCondition(Condition):
    """Compare a data key against a value with a binary operator."""
    def __init__(self, key: str, operator_name, value):
        self.key = key
        self.operator_name = operator_name
        self.value = value

    def to_filter(self):
        if self.operator_name == "ne":
            eq_filter = rbh.build_filter([f"-{self.key}", str(self.value)])
            return rbh.rbh_filter_not(eq_filter)
        # In this patch we provide the key and value to create the filter, but
        # it won't work. We need a translator that acts as an intermediary
        # between the configuration interface we're using and what rbh-find
        # understands. We'll do this in future patches.
        return rbh.build_filter([f"-{self.key}", str(self.value)])

    def __repr__(self):
        op_symbol = {
            "eq": "==",
            "ne": "!=",
            "lt": "<",
            "le": "<=",
            "gt": ">",
            "ge": ">="
        }
        op_symbol.get(self.operator_name, self.operator_name)
        return f"{self.key} {op_symbol} {self.value}"

class AndCondition(Condition):
    """Logical AND of two conditions."""
    def __init__(self, left: Condition, right: Condition):
        self.left = left
        self.right = right

    def to_filter(self):
        return rbh.rbh_filter_and(self.left.to_filter(), self.right.to_filter())

    def __repr__(self):
        return f"({self.left!r} & {self.right!r})"

class OrCondition(Condition):
    """Logical OR of two conditions."""
    def __init__(self, left: Condition, right: Condition):
        self.left = left
        self.right = right

    def to_filter(self):
        return rbh.rbh_filter_or(self.left.to_filter(), self.right.to_filter())

    def __repr__(self):
        return f"({self.left!r} | {self.right!r})"


class NotCondition(Condition):
    """Logical NOT of a condition."""
    def __init__(self, cond: Condition):
        self.cond = cond

    def to_filter(self):
        return rbh.rbh_filter_not(self.cond.to_filter())

    def __repr__(self):
        return f"(~({self.cond!r}))"
