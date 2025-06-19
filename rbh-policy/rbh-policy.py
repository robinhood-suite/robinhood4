#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.config_loader import load_config

if __name__ == "__main__":
    fs_name = "fs1"
    config = load_config(fs_name)

    print(f"Loaded configuration for {fs_name}: {config}")
    print("Hello, I'm PE")
