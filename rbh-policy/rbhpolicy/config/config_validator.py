#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.models.conditions import Condition


def validate_fileclass(name, condition):
    """
    Validate arguments for declare_fileclass.
    Raise explicit errors if types or values are incorrect.
    """
    if not isinstance(name, str):
        raise TypeError(f"FileClass name must be a string, got "
                        f"{type(name).__name__}")

    if not isinstance(condition, Condition):
        raise TypeError(f"FileClass condition must be a Condition instance, got "
                        f"{type(condition).__name__}")
