# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

from corepolicyengine import *

def load_config(config_path):
    module_name = os.path.splitext(os.path.basename(config_path))[0]
    return importlib.import_module(module_name)

def evaluate_target(target, file_data):
    return target.evaluate(file_data)

def execute_policy(config_module, policy_name):
    if policy_name not in config_module.policies:
        print(f"Policy '{policy_name}' not found.")
        sys.exit(1)
    policy = config_module.policies[policy_name]
    print(f"Policy: {policy_name}")
    print(f"  - Target: {policy['target']}")
    print(f"  - Action: {policy['action'].__name__}")
    print(f"  - Parameters: {policy['parameters']}")
    sample_data = {
        "Size": 600,
        "LastAccess": 366,
        "LastModification": 20,
        "User": "usr7"
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
    config_module = load_config(config_file)
    execute_policy(config_module, policy_name)
