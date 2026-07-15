#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --last blob &&
        error "log with invalid last count should have failed"

    rbh_log "rbh:$db:$testdb" --last 42invalid &&
        error "log with invalid last count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    echo "$output" | grep "Start" > /dev/null ||
        error "start_time should have been retrieved"

    echo "$output" | grep "Duration" > /dev/null ||
        error "duration should have been retrieved"

    echo "$output" | grep "End" > /dev/null ||
        error "end_time should have been retrieved"

    local mountpoint=$(echo "$output" | grep "Mountpoint used" |
                       cut -d':' -f2- | xargs)

    if [ "$mountpoint" != "$(pwd)" ]; then
        error "Invalid mountpoint"
    fi

    local command_line=$(grep 'Command used' <<< "$output" |
                         sed -n 's/.*rbh-sync/rbh-sync/p')

    if [ "$command_line" != "rbh-sync rbh:posix:. rbh:$db:$testdb" ];
    then
        error "command lines are not matching"
    fi

    local converted_entries=$(echo "$output" | grep "converted" |
                              cut -d':' -f2- |xargs)

    local skipped_entries=$(echo "$output" | grep "skipped" |
                            cut -d':' -f2- |xargs)

    local total_entries=$(echo "$output" | grep "Total entries seen" |
                          cut -d':' -f2- |xargs)

    local sum_entries=$((skipped_entries + converted_entries))

    local find_entries=$(find . | wc -l)

    if [ "$sum_entries" != "$find_entries" ]; then
        error "The sum of converted and skipped entries does not match the
               number of entries inside the directory"
    fi

    if [ "$sum_entries" != "$total_entries" ]; then
        error "The sum of converted and skipped entries does not match
               total_entries_seen"
    fi
}

test_N_logs()
{
    local requested=$1
    local expected=$2

    for i in $(seq 1 $expected); do
        rbh_sync "rbh:posix:." "rbh:$db:$testdb"
        sleep 1
    done

    local output=$(rbh_log "rbh:$db:$testdb" --last $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 10 * $expected)); then
        error "There should be 10 * $expected lines about posix sync (8 for content, 2 for header/footer, time $expected logs), got '$output'"
    fi

    for i in $(seq 1 $expected); do
        local one_log="$(echo "$output" | head -n 10)"
        check_log_result "$one_log"
        output="$(echo "$output" | sed 1,10d)"
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
