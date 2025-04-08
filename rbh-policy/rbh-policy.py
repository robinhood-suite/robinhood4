#!/usr/bin/env python3
'''
This file is part of RobinHood 4
Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                   alternatives

SPDX-License-Identifer: LGPL-3.0-or-later
'''

import importlib.util
import argparse
import types
import sys
import os

# Condition logic

class ConfigCondition:
    def __init__(self, key, op_str=None):
        self.key = key  # string or lambda
        self.op_str = op_str  # for printing

    def _wrap(self, func, op_str):
        return ConfigCondition(func, op_str)

    def _get(self, data):
        return self.key(data) if callable(self.key) else data[self.key]

    def evaluate(self, data):
        return self._get(data) if callable(self.key) else data[self.key]

    def __eq__(self, other):
        return self._wrap(lambda data: self._get(data) == other,
                          f"({self} == {other})")

    def __gt__(self, other):
        return self._wrap(lambda data: self._get(data) > other,
                          f"({self} > {other})")

    def __lt__(self, other):
        return self._wrap(lambda data: self._get(data) < other,
                          f"({self} < {other})")

    def __ge__(self, other):
        return self._wrap(lambda data: self._get(data) >= other,
                          f"({self} >= {other})")

    def __le__(self, other):
        return self._wrap(lambda data: self._get(data) <= other,
                          f"({self} <= {other})")

    def __and__(self, other):
        return self._wrap(lambda data: self.evaluate(data) and
                          other.evaluate(data), f"({self} & {other})")

    def __or__(self, other):
        return self._wrap(lambda data: self.evaluate(data) or
                          other.evaluate(data), f"({self} | {other})")

    def __invert__(self):
        return self._wrap(lambda data: not self.evaluate(data), f"({self})")

    def __repr__(self):
        return self.op_str if self.op_str else f"Condition({self.key})"


class FileClassCondition:
    def __init__(self, name, condition):
        self.name = name
        self.condition = condition

    def evaluate(self, data):
        return self.condition.evaluate(data)

    def __and__(self, other):
        return ConfigCondition(lambda data: self.evaluate(data) and
                               other.evaluate(data))

    def __or__(self, other):
        return ConfigCondition(lambda data: self.evaluate(data) or
                               other.evaluate(data))

    def __invert__(self):
        return ConfigCondition(lambda data: not self.evaluate(data))

    def __repr__(self):
        return f"FileClass({self.name})"

# Globals for config

fileclasses = {}
policies = {}

def declare_fileclass(name, condition):
    fc = FileClassCondition(name, condition)
    fileclasses[name] = fc

    # Inject into the caller's module
    caller_module = sys._getframe(1).f_globals
    caller_module[name] = fc

    return fc

def declare_policy(name, target, action, parameters=None, trigger=None,
                   rules=None):
    policies[name] = {
        "name": name,
        "target": target,
        "action": action,
        "parameters": parameters or {},
        "trigger": trigger,
        "rules": rules or []
    }

# Available variables in config

Size = ConfigCondition("Size")
Last_Access = ConfigCondition("Last_Access")
LastModification = ConfigCondition("LastModification")

# Available actions

def archive_files(**kwargs):
    print(f"[ACTION] archive_files with {kwargs}")

def delete_files(**kwargs):
    print(f"[ACTION] delete_files with {kwargs}")

# Load config.py

def load_config(config_path):
    spec = importlib.util.spec_from_file_location("config", config_path)
    config = types.ModuleType(spec.name)

    # Inject functions and global variables into the config module
    config.__dict__.update({
        "declare_fileclass": declare_fileclass,
        "declare_policy": declare_policy,
        "Size": Size,
        "Last_Access": Last_Access,
        "LastModification": LastModification,
        "archive_files": archive_files,
        "delete_files": delete_files,
    })

    spec.loader.exec_module(config)
    for key, value in config.__dict__.items():
        if not key.startswith("__"):
            globals()[key] = value

# Evaluate target condition

def evaluate_target(target, file_data):
    return target.evaluate(file_data)

# Execute a policy

def execute_policy(policy_name):
    if policy_name not in policies:
        print(f"Policy '{policy_name}' not found.")
        sys.exit(1)

    policy = policies[policy_name]
    print(f"Policy: {policy_name}")
    print(f"  - Target: {policy['target']}")
    print(f"  - Action: {policy['action'].__name__}")
    print(f"  - Parameters: {policy['parameters']}")

    sample_data = {
        "Size": 600,
        "Last_Access": 180,
        "LastModification": 20
    }

    print("Sample data:", sample_data)

    if not evaluate_target(policy["target"], sample_data):
        print("Target condition not met, no action executed")
        return

    rules = policy.get("rules", [])
    for rule in rules:
        print(f"  - Evaluating rule: {rule['name']}")
        if evaluate_target(rule["target"], sample_data):
            print(f"    ✔ Rule '{rule['name']}' matched, executing" +
                    " override action")
            action = rule.get("action", policy["action"])
            parameters = rule.get("parameters", policy["parameters"])
            action(**parameters)
            return

    print("No rule matched, applying default action")
    policy["action"](**policy["parameters"])


# CLI

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="This command is used to" +
                                                  " execute robinhood policies.")
    parser.add_argument("-c", "--config", type=str, required=True,
                        help="Path to the configuration file.")
    parser.add_argument("policy", type=str,
                        help="The name of the policy to execute.")
    args = parser.parse_args()

    if not args.config or not args.policy:
        parser.print_help()
        sys.exit(1)

    config_file = args.config
    policy_name = args.policy

    if not os.path.exists(config_file):
        print(f"Configuration file not found: {config_file}")
        sys.exit(1)

    load_config(config_file)
    execute_policy(policy_name)
