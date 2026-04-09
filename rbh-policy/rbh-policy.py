#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from rbhpolicy.config.config_loader import load_config
from rbhpolicy.execution.run import run
from rbhpolicy.execution.show import show
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RBH Policy CLI")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run",
            help="Run policies on a filesystem")
    run_parser.add_argument("fs_name",
            help="Path to the configuration file or filesystem name")
    run_parser.add_argument("policies",
            help="Comma-separated list of policies to run")

    show_parser = subparsers.add_parser("show",
            help="Show policies or fileclasses from a configuration")
    show_parser.add_argument("kind", choices=["policy", "fileclass"],
            help="Object type to show")
    show_parser.add_argument("fs_name",
            help="Path to the configuration file or filesystem name")
    show_parser.add_argument("names", nargs="?",
            help="Comma-separated list of object names to show (optional)")

    args = parser.parse_args()

    if args.command == "run":
        fs_name = args.fs_name
        policy_list = args.policies.split(",")
        config = load_config(fs_name)
        print(f"Loaded configuration for '{fs_name}'")
        run(policy_list)
    elif args.command == "show":
        fs_name = args.fs_name
        selected_names = args.names.split(",") if args.names else None
        config = load_config(fs_name)
        show(args.kind, selected_names)
