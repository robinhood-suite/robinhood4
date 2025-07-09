#!/usr/bin/env python3.9
'''
This file is part of RobinHood 4
Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                   alternatives
SPDX-License-Identifer: LGPL-3.0-or-later
'''

from rbhpolicy.config.config_loader import load_config


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <filesystem_name>")
        sys.exit(1)

    fs_name = sys.argv[1]
    load_config(fs_name)

    print(f"Loaded configuration for {fs_name}")
    print("Hello, I'm PE")
