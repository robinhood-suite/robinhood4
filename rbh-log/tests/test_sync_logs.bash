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

    rbh_log "rbh:$db:$testdb" --first-sync --last-sync &&
        error "log with both first and last sync should have failed"
}

test_collection_sync()
{
    local info_flag=$1

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output=$(rbh_log "rbh:$db:$testdb" "$info_flag")
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 9)); then
        error "There should be eight infos about posix sync + 1 line for the header, got '$output'"
    fi

    echo "$output" | grep "Start" ||
        error "sync_debut should have been retrieved"

    echo "$output" | grep "Duration" ||
        error "sync_duration should have been retrieved"

    echo "$output" | grep "End" ||
        error "sync_end should have been retrieved"

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

test_collection_first_sync() {
    test_collection_sync "--first-sync"
}

test_collection_last_sync() {
    test_collection_sync "--last-sync"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_collection_first_sync test_collection_last_sync)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
