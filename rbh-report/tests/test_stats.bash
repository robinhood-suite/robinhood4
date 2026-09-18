#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

find_expected_values()
{
    local output="$1"

    echo "$output" | grep "rbh-report" > /dev/null ||
        error "Should have found 'rbh-report' mentionned, got '$output'"
}

test_stats()
{
    mkdir -p {1..9}/{1..9}

    rbh_sync rbh:posix:. rbh:$db:$testdb

    local output="$(rbh_report --stats rbh:$db:$testdb --output "count()" 2>&1)"
    find_expected_values "$output"

    rbh_report --stats --log-file logs.txt rbh:$db:$testdb --output "count()"
    output="$(cat logs.txt)"
    find_expected_values "$output"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_stats)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
