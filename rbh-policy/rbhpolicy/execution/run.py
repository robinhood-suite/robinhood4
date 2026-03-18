#!/usr/bin/env python3
# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

from rbhpolicy.config.policy import rbh_policies
from rbhpolicy.config.cpython import (
    collect_fs_entries,
    get_database,
    rbh_pe_execute,
)
from rbhpolicy.config.triggers.triggers import TriggerContext, evaluate_trigger

def run(policies):
    unknown = [name for name in policies if name not in rbh_policies]
    if unknown:
        raise ValueError(f"Unknown policy name(s): {', '.join(unknown)}")

    for name in policies:
        policy = rbh_policies[name]
        decision = evaluate_trigger(
            policy.trigger,
            TriggerContext(manual_mode=True, database_uri=get_database()),
        )

        if not decision.matched:
            print(
                f"[INFO] Skipping policy '{name}': trigger not matched "
                f"({decision.reason})"
            )
            continue

        print(f"[INFO] Executing policy '{name}'")
        rbhfilter = policy._filter
        iterator, backend = collect_fs_entries(rbhfilter)

        result = rbh_pe_execute(iterator, backend, policy)
        print(f"[INFO] Policy '{name}' completed")
