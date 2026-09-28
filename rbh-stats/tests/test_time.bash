#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_stats.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_stats rbh:$db:$testdb --before blob &&
        error "Parsing of timestamp 'blob' should have failed"

    rbh_stats rbh:$db:$testdb --before -3 &&
        error "Parsing of timestamp '-3' should have failed"

    rbh_stats rbh:$db:$testdb --before 0 &&
        error "Timestamp '0' should have triggered an error"

    return 0
}

test_after()
{
    local time1="$(date +%s)"
    sleep 1
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    local time2="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    local time3="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    local time4="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_stats rbh:$db:$testdb -n 10 --after $time2 --csv)"
    echo "$output" | wc -l | grep "3" ||
        error "Should have found 3 stats after '$time2', got '$output'"

    output="$(rbh_stats rbh:$db:$testdb -n 10 --after $time1 --csv)"
    echo "$output" | wc -l | grep "4" ||
        error "Should have found 4 stats after '$time1', got '$output'"

    sleep 1
    local time5="$(date +%s)"

    output="$(rbh_stats rbh:$db:$testdb -n 10 --after $time5 --csv)"
    if [ ! -z "$output" ]; then
        error "Should have found 0 stats after '$time5', got '$output'"
    fi
}

test_before()
{
    local time1="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    local time2="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    local time3="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    local time4="$(date +%s)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_stats rbh:$db:$testdb -n 10 --before $time3 --csv)"
    echo "$output" | wc -l | grep "2" ||
        error "Should have found 2 stats before '$time3', got '$output'"

    output="$(rbh_stats rbh:$db:$testdb -n 10 --before $time1 --csv)"
    if [ ! -z "$output" ]; then
        error "Should have found 0 stats before '$time1', got '$output'"
    fi

    sleep 1
    local time5="$(date +%s)"

    output="$(rbh_stats rbh:$db:$testdb -n 10 --before $time5 --csv)"
    echo "$output" | wc -l | grep "4" ||
        error "Should have found 4 stats before '$time5', got '$output'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_after test_before)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
