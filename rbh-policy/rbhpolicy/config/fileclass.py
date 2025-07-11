#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.conditions import LogicalCondition
import sys

rbh_fileclasses = {}

class FileClass(LogicalCondition):
    """A named file‐class defined by a Condition."""
    def __init__(self, name: str, condition: LogicalCondition):
        self.name = name
        self.condition = condition
        self._filter = None

    def to_filter(self):
        if self._filter is None:
            self._filter = self.condition.to_filter()
        return self._filter

    def __repr__(self) -> str:
        return f"FileClass({self.name})"


def declare_fileclass(*, name: str, target: LogicalCondition) -> FileClass:
    """
    Declare a new file‐class in the global registry and inject it
    into the caller’s namespace so it’s directly available in config files.
    """
    fc = FileClass(name, target)
    fc._filter = target.to_filter()
    rbh_fileclasses[name] = fc

    # Inject the created FileClass into the caller’s global scope for
    # direct use by name
    caller_ns = sys._getframe(1).f_globals
    caller_ns[name] = fc

    return fc
