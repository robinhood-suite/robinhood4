#!/usr/bin/env bash

# This file is part of RobinHood.
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

    rbh_log "rbh:$db:$testdb" --last blob &&
        error "log with invalid last count should have failed"

    rbh_log "rbh:$db:$testdb" --last 42invalid &&
        error "log with invalid last count should have failed"

    return 0
}

check_sync_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-sync "rbh-sync rbh:posix:. rbh:$db:$testdb"

    local mountpoint=$(echo "$output" | grep "Mountpoint used" |
                       cut -d':' -f2- | xargs)

    if [ "$mountpoint" != "$(pwd)" ]; then
        error "Invalid mountpoint"
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

check_find_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-find "rbh-find rbh:$db:$testdb"

    echo "$output" | grep "post-filtering" > /dev/null ||
        error "entry_count should have been retrieved"
}

test_last_logs()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sleep 1
    rbh_find "rbh:$db:$testdb" > /dev/null

    local output=$(rbh_log "rbh:$db:$testdb" --last 2)
    local tmp_output=$(rbh_log "rbh:$db:$testdb" --last 20)

    if [ "$output" != "$tmp_output" ]; then
        error "Outputted logs should have been the same, got '$output' and '$tmp_output'"
    fi

    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 10 + 7)); then
        error "There should be 10 lines about sync (8 for content, 2 for header/footer) + 7 for find (5/2 aswell), got '$output'"
    fi

    local find_log="$(echo "$output" | head -n 7)"
    check_find_log_result "$find_log"
    output="$(echo "$output" | sed 1,7d)"

    local sync_log="$(echo "$output" | head -n 10)"
    check_sync_log_result "$sync_log"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_last_logs)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
