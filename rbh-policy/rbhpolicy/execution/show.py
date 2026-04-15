#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from rbhpolicy.config.policy import rbh_policies
from rbhpolicy.config.fileclass import rbh_fileclasses
import re

def select_items(registry, requested_names, object_label):
    if requested_names:
        names_to_show = [name for name in requested_names if name in registry]
        missing = [name for name in requested_names if name not in registry]
        if missing:
            print(f"Unknown {object_label}(s): {', '.join(missing)}")
    else:
        names_to_show = list(registry.keys())

    return names_to_show

def show_policies(policy_names=None):
    if not rbh_policies:
        print("No policy found in configuration.")
        return

    policies_to_show = select_items(rbh_policies, policy_names, "policy")

    for name in policies_to_show:
        policy = rbh_policies[name]
        print(f"\nPolicy: {policy.name}")
        print(f"  Target: {repr(policy.target)}")

        action_name = getattr(policy.action, '__name__', str(policy.action))
        print(f"  Action: {action_name}")

        print(f"  Trigger: {str(policy.trigger)}")

        if policy.parameters:
            print("  Parameters:")
            for key, value in policy.parameters.items():
                print(f"    {key}: {value}")
        else:
            print("  Parameters: {}")

        if policy.sort:
            print("  Sort:")
            print(f"    sort_by: {policy.sort['sort_by']}")
            print(f"    sort_order: {policy.sort['sort_order']}")

        if policy.rules:
            print("  Rules:")
            for rule in policy.rules:
                rule_action_name = getattr(rule.action, '__name__', str(rule.action))
                print(f"    - Rule: {rule.name}")
                print(f"      Condition: {repr(rule.condition)}")
                if rule.action:
                    print(f"      Action: {rule_action_name}")
                if rule.parameters:
                    print("      Parameters:")
                    for param_key, param_value in rule.parameters.items():
                        print(f"        {param_key}: {param_value}")
        else:
            print("  Rules: []")


def show_fileclasses(fileclass_names=None):
    if not rbh_fileclasses:
        print("No fileclass found in configuration.")
        return

    fileclasses_to_show = select_items(
        rbh_fileclasses, fileclass_names, "fileclass"
    )

    for name in fileclasses_to_show:
        fileclass = rbh_fileclasses[name]
        print(f"\nFileClass: {fileclass.name}")
        print(f"  Target: {repr(fileclass.condition)}")


def show(kind="policy", names=None):
    if kind == "policy":
        show_policies(names)
    elif kind == "fileclass":
        show_fileclasses(names)
    else:
        print(f"Unsupported show kind: {kind}")
