#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from rbhpolicy.config.policy import rbh_policies
from rbhpolicy.config.fileclass import FileClass, rbh_fileclasses
from rbhpolicy.config.conditions import (
    AndCondition,
    NotCondition,
    OrCondition,
    SimpleCondition,
)
from rbhpolicy.config.triggers.triggers import (
    AlwaysTrigger,
    DbStatTrigger,
    GlobalInodePercentTrigger,
    GlobalSizePercentTrigger,
    PeriodicTrigger,
    ScheduledTrigger,
    TriggerAnd,
    TriggerOr,
)
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


def format_condition(condition):
    if isinstance(condition, FileClass):
        return format_condition(condition.condition)

    if isinstance(condition, SimpleCondition):
        return str(condition)

    if isinstance(condition, AndCondition):
        return (f"({format_condition(condition.left)} & "
            f"{format_condition(condition.right)})")

    if isinstance(condition, OrCondition):
        return (f"({format_condition(condition.left)} | "
            f"{format_condition(condition.right)})")

    if isinstance(condition, NotCondition):
        return f"(~({format_condition(condition.cond)}))"

    return str(condition)


def format_duration(seconds):
    if seconds % 86400 == 0:
        return f"{seconds // 86400}d"
    if seconds % 3600 == 0:
        return f"{seconds // 3600}h"
    if seconds % 60 == 0:
        return f"{seconds // 60}m"
    return f"{seconds}s"


def format_dbstat_label(label):
    match = re.match(r"^([^\[]+)\[(.*)\]$", label)
    if not match:
        return label

    metric_name, selectors = match.groups()
    return f"{metric_name}({selectors})"


def format_trigger(trigger):
    if isinstance(trigger, AlwaysTrigger):
        return "Always"

    if isinstance(trigger, PeriodicTrigger):
        return f"Periodic {format_duration(trigger.interval_seconds)}"

    if isinstance(trigger, ScheduledTrigger):
        return trigger.target_datetime.strftime("Scheduled %Y-%m-%d %H:%M:%S")

    if isinstance(trigger, GlobalSizePercentTrigger):
        return f"GlobalSizePercent {trigger.operator} {trigger.threshold}"

    if isinstance(trigger, GlobalInodePercentTrigger):
        return f"GlobalInodePercent {trigger.operator} {trigger.threshold}"

    if isinstance(trigger, DbStatTrigger):
        metric = format_dbstat_label(trigger.label)
        return f"{metric} {trigger.operator} {trigger.threshold}"

    if isinstance(trigger, TriggerAnd):
        return (f"({format_trigger(trigger.left)} AND "
            f"{format_trigger(trigger.right)})")

    if isinstance(trigger, TriggerOr):
        return (f"({format_trigger(trigger.left)} OR "
            f"{format_trigger(trigger.right)})")

    return str(trigger)


def show_policies(policy_names=None):
    if not rbh_policies:
        print("No policy found in configuration.")
        return

    policies_to_show = select_items(rbh_policies, policy_names, "policy")

    for name in policies_to_show:
        policy = rbh_policies[name]
        print(f"\nPolicy: {policy.name}")
        print(f"  Target: {format_condition(policy.target)}")

        action_name = getattr(policy.action, '__name__', str(policy.action))
        print(f"  Action: {action_name}")

        print(f"  Trigger: {format_trigger(policy.trigger)}")

        if policy.parameters:
            print("  Parameters:")
            for key, value in policy.parameters.items():
                print(f"    {key}: {value}")
        else:
            print("  Parameters: {}")

        if policy.rules:
            print("  Rules:")
            for rule in policy.rules:
                rule_action_name = getattr(rule.action, '__name__', str(rule.action))
                print(f"    - Rule: {rule.name}")
                print(f"      Condition: {format_condition(rule.condition)}")
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
        print(f"  Target: {format_condition(fileclass.condition)}")


def show(kind="policy", names=None):
    if kind == "policy":
        show_policies(names)
    elif kind == "fileclass":
        show_fileclasses(names)
    else:
        print(f"Unsupported show kind: {kind}")
