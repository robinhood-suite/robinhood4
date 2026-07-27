#!/usr/bin/env bash

# This file is part of RobinHood4.
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

    rbh_log "rbh:$db:$testdb" --gc blob &&
        error "log with invalid gc count should have failed"

    rbh_log "rbh:$db:$testdb" --gc 42invalid &&
        error "log with invalid gc count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-gc "rbh-gc rbh:$db:$testdb -size +1M -s 42"

    echo "$output" | grep " deleted " > /dev/null ||
        error "deleted_entries should have been retrieved, got '$output'"

    echo "$output" | grep "non-deleted" > /dev/null ||
        error "non_deleted_entries should have been retrieved, got '$output'"

    echo "$output" | grep "Sync" > /dev/null ||
        error "sync_time should have been retrieved, got '$output'"

    echo "$output" | grep "seen" > /dev/null ||
        error "total_entries should have been retrieved, got '$output'"

}

test_N_logs()
{
    local requested=$1
    local expected=$2
    local count=10

    rbh_sync rbh:posix:. rbh:$db:$testdb
    sleep 1
    for i in $(seq 1 $expected); do
        rbh_gc "rbh:$db:$testdb" -size +1M -s 42
        sleep 1
    done

    local output=$(rbh_log "rbh:$db:$testdb" --gc $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != $count * $expected)); then
        error "There should be $count * $expected lines about gc ($(($count - 2)) for content, 2 for header/footer, time $expected logs), got '$output'"
    fi

    for i in $(seq 1 $expected); do
        local one_log="$(echo "$output" | head -n $count)"
        check_log_result "$one_log"
        output="$(echo "$output" | sed 1,${count}d)"
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
