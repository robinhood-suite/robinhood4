#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash
. $test_dir/lustre_utils.bash

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

# Each of the following functions will check the associated command look to be
# outputting the correct lines, and return the whole output with the current
# command truncated.
#
# The "|| true" at the end of each functions is necessary because tail outputs
# an error if there are not enough lines to read, which should be the case for
# the very last log.

check_rbh_sync()
{
    local output="$1"

    local count=10
    local output="$(echo "$full_output" | head -n $count)"

    check_common_logs "$output" rbh-sync "rbh-sync rbh:posix:. rbh:$db:$testdb"

    echo "$output" | grep "Mountpoint" > /dev/null ||
        error "source_mountpoint should have been retrieved"

    echo "$output" | grep "converted" > /dev/null ||
        error "converted_entries should have been retrieved"

    echo "$output" | grep "skipped" > /dev/null ||
        error "skipped_entries should have been retrieved"

    echo "$output" | grep "seen" > /dev/null ||
        error "total_entries should have been retrieved"

    echo "$full_output" | tail +$count || true
}

check_rbh_find()
{
    local full_output="$1"

    local count=8
    local output="$(echo "$full_output" | head -n $count)"

    check_common_logs "$output" rbh-find \
        "rbh-find rbh:$db:$testdb -exec ls ;"

    echo "$output" | grep "post-filtering" > /dev/null ||
        error "entry_count should have been retrieved"

    echo "$output" | grep "exec" > /dev/null ||
        error "exec_success_count should have been retrieved"

    echo "$full_output" | tail +$count || true
}

check_rbh_fsevents()
{
    local full_output="$1"

    local count=15
    local output="$(echo "$full_output" | head -n $count)"

    check_common_logs "$output" rbh-fsevents \
        "rbh-fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb"

    echo "$output" | grep "Enrichment" > /dev/null ||
        error "enrich_mountpoint should have been retrieved, got '$output'"

    echo "$output" | grep "Source" > /dev/null ||
        error "source_read should have been retrieved, got '$output'"

    echo "$output" | grep "Number" > /dev/null ||
        error "worker_count should have been retrieved, got '$output'"

    echo "$output" | grep "Amount" > /dev/null ||
        error "changelog_read should have been retrieved, got '$output'"

    echo "$output" | grep "Starting" > /dev/null ||
        error "start_index should have been retrieved, got '$output'"

    echo "$output" | grep "reading/deduplicating" > /dev/null ||
        error "time_read_dedup should have been retrieved, got '$output'"

    echo "$output" | grep "enriching/updating" > /dev/null ||
        error "time_enrich_update should have been retrieved, got '$output'"

    echo "$output" | grep "skipped" > /dev/null ||
        error "enrich_skip_count should have been retrieved, got '$output'"

    echo "$output" | grep "Ratio" > /dev/null ||
        error "deduplication_ratio should have been retrieved, got '$output'"

    echo "$full_output" | tail +$count || true
}

check_rbh_report()
{
    local full_output="$1"

    local count=6
    local output="$(echo "$full_output" | head -n $count)"

    check_common_logs "$output" rbh-report \
        "rbh-report rbh:$db:$testdb --group-by statx.uid --output sum(statx.size)"

    echo "$full_output" | tail +$count || true
}

check_rbh_gc()
{
    local full_output="$1"

    local count=10
    local output="$(echo "$full_output" | head -n $count)"

    check_common_logs "$output" rbh-gc "rbh-gc rbh:$db:$testdb --sync-time 42"

    echo "$output" | grep " deleted " > /dev/null ||
        error "deleted_entries should have been retrieved, got '$output'"

    echo "$output" | grep "non-deleted" > /dev/null ||
        error "non_deleted_entries should have been retrieved, got '$output'"

    echo "$output" | grep "Sync" > /dev/null ||
        error "sync_time should have been retrieved, got '$output'"

    echo "$output" | grep "seen" > /dev/null ||
        error "total_entries should have been retrieved, got '$output'"

    echo "$full_output" | tail +$count || true
}

test_last_logs()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    # Output 20 random ints between 0 and 4
    for i in $(shuf -i 0-4 -r -n 20); do
        case "$i" in
            0)
                rbh_sync rbh:posix:. rbh:$db:$testdb
                ;;
            1)
                rbh_find rbh:$db:$testdb -exec ls \; > /dev/null
                ;;
            2)
                rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR \
                    src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
                ;;
            3)
                rbh_report rbh:$db:$testdb \
                    --group-by "statx.uid" \
                    --output "sum(statx.size)" > /dev/null
                ;;
            4)
                rbh_gc rbh:$db:$testdb --sync-time 42
                ;;
        esac
    done

    local output=$(rbh_log "rbh:$db:$testdb" --last 21)
    local tmp_output=$(rbh_log "rbh:$db:$testdb" --last 30)

    if [ "$output" != "$tmp_output" ]; then
        error "Outputted logs should have been the same, got '$output' and '$tmp_output'"
    fi

    while [ ! -z "$output" ]; do
        local command="$(echo "$output" | head -n 1)"

        if [[ $command == *"rbh-sync"* ]]; then
            output="$(check_rbh_sync "$output")"
        elif [[ $command == *"rbh-find"* ]]; then
            output="$(check_rbh_find "$output")"
        elif [[ $command == *"rbh-fsevents"* ]]; then
            output="$(check_rbh_fsevents "$output")"
        elif [[ $command == *"rbh-report"* ]]; then
            output="$(check_rbh_report "$output")"
        elif [[ $command == *"rbh-gc"* ]]; then
            output="$(check_rbh_gc "$output")"
        fi
    done
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_last_logs)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
