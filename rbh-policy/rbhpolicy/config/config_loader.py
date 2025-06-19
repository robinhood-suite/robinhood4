#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import sys
import os
import importlib

CONFIG_DIR = "/etc/robinhood4.d/"

def load_config(fs_name):
    """Imports the config module named fs_name from CONFIG_DIR"""
    if CONFIG_DIR not in sys.path:
        sys.path.insert(0, CONFIG_DIR)

    return importlib.import_module(fs_name)
