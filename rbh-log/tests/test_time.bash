#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_log rbh:$db:$testdb --before blob &&
        error "Parsing of timestamp 'blob' should have failed"

    rbh_log rbh:$db:$testdb --before -3 &&
        error "Parsing of timestamp '-3' should have failed"

    rbh_log rbh:$db:$testdb --before 0 &&
        error "Timestamp '0' should have triggered an error"

    return 0
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

    rbh_log rbh:$db:$testdb -n 10 --before $time3 --csv
    local output="$(rbh_log rbh:$db:$testdb -n 10 --before $time3 --csv)"
    echo "$output" | wc -l | grep "2" ||
        error "Should have found 2 logs before '$time3', got '$output'"

    output="$(rbh_log rbh:$db:$testdb -n 10 --before $time1 --csv)"
    if [ ! -z "$output" ]; then
        error "Should have found 0 logs before '$time1', got '$output'"
    fi

    sleep 1
    local time5="$(date +%s)"

    output="$(rbh_log rbh:$db:$testdb -n 10 --before $time5 --csv)"
    echo "$output" | wc -l | grep "4" ||
        error "Should have found 4 logs before '$time5', got '$output'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_before)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
