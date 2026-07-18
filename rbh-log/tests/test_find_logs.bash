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
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --find blob &&
        error "log with invalid find count should have failed"

    rbh_log "rbh:$db:$testdb" --find 42invalid &&
        error "log with invalid find count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"
    local with_exec="$2"

    if [ "$with_exec" == "true" ]; then
        check_common_logs "$output" rbh-find \
            "rbh-find rbh:$db:$testdb -exec ls {} ;"
    else
        check_common_logs "$output" rbh-find "rbh-find rbh:$db:$testdb"
    fi

    echo "$output" | grep "post-filtering" > /dev/null ||
        error "entry_count should have been retrieved"

    if [ "$with_exec" == "true" ]; then
        echo "$output" | grep "exec" > /dev/null ||
            error "exec_success_count should have been retrieved"
    fi
}

test_N_logs()
{
    local requested=$1
    local expected=$2

    local with_exec=$(( expected / 2 ))
    local without_exec=$(( expected - with_exec ))
    local expected_line_count=0

    rbh_sync rbh:posix:. rbh:$db:$testdb
    for i in $(seq 1 $with_exec); do
        rbh_find "rbh:$db:$testdb" -exec ls "{}" \; > /dev/null
        sleep 1
        expected_line_count=$(( expected_line_count + 6 + 2 ))
    done

    for i in $(seq 1 $without_exec); do
        rbh_find "rbh:$db:$testdb" > /dev/null
        sleep 1
        expected_line_count=$(( expected_line_count + 5 + 2 ))
    done

    local output=$(rbh_log "rbh:$db:$testdb" --find $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != $expected_line_count)); then
        error "There should be $expected_line_count lines about find, got '$output'"
    fi

    for i in $(seq 1 $without_exec); do
        local one_log="$(echo "$output" | head -n 7)"
        check_log_result "$one_log" false
        output="$(echo "$output" | sed 1,7d)"
    done

    for i in $(seq 1 $with_exec); do
        local one_log="$(echo "$output" | head -n 8)"
        check_log_result "$one_log" true
        output="$(echo "$output" | sed 1,8d)"
    done
}

test_last_1()
{
    test_N_logs 1 1
}

test_last_N()
{
    test_N_logs 3 3
}

test_more_than_N()
{
    test_N_logs 6 3
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_last_1 test_last_N test_more_than_N)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
