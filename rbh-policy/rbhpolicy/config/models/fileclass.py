#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.models.conditions import Condition
import sys

rbh_fileclasses = {}

class FileClass:
    """A named file‐class defined by a Condition."""
    def __init__(self, name: str, condition: Condition):
        self.name = name
        self.condition = condition

    def evaluate(self, data: dict) -> bool:
        return self.condition.evaluate(data)

    def __and__(self, other: Condition) -> Condition:
        return self.condition & other

    def __or__(self, other: Condition) -> Condition:
        return self.condition | other

    def __invert__(self) -> Condition:
        return ~self.condition

    def __repr__(self) -> str:
        return f"FileClass({self.name})"


def declare_fileclass(name: str, condition: Condition) -> FileClass:
    """
    Declare a new file‐class in the global registry and inject it
    into the caller’s namespace so it’s directly available in config files.
    """
    fc = FileClass(name, condition)
    rbh_fileclasses[name] = fc

    caller_ns = sys._getframe(1).f_globals
    caller_ns[name] = fc

    return fc
