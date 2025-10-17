#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import sys
import os
import importlib

class Directory():
    """Imports the config module named fs_name from CONFIG_DIR"""
    def __init__(self, name):
        self.name = name
