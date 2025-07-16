#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.conditions import LogicalCondition


def validate_fileclass(name, condition):
    """
    Validate arguments for declare_fileclass.
    Raise explicit errors if types or values are incorrect.
    """
    if not isinstance(name, str):
        raise TypeError(f"FileClass name must be a string, got "
                        f"{type(name).__name__}")

    if not isinstance(condition, LogicalCondition):
        raise TypeError(f"FileClass condition must be a LogicalCondition "
                        f"instance, got {type(condition).__name__}")
