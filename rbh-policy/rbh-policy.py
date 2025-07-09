#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from rbhpolicy.config.config_loader import load_config
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Load configuration for a "
                                                 "filesystem")
    parser.add_argument('fs-config',
            help="Name of the filesystem to load config for")

    args = parser.parse_args()
    fs_name = args.fs-config

    load_config(fs_name)
    print(f"Loaded configuration for {fs_name}")
    print("Hello, I'm PE")
