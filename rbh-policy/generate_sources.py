#!/usr/bin/env python3.9
'''
This file is part of RobinHood 4
Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                   alternatives
SPDX-License-Identifer: LGPL-3.0-or-later
'''
import os

this_dir = os.path.dirname(os.path.abspath(__file__))
base = os.path.join(this_dir, 'rbhpolicy')

for root, _, files in os.walk(base):
    for f in files:
        if f.endswith('.py'):
            full = os.path.join(root, f)
            subdir = os.path.relpath(os.path.dirname(full), this_dir)
            print(f"{subdir}:{full}")
