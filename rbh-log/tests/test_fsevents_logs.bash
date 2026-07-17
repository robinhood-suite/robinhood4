#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash
. $test_dir/common_logs.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --fsevents blob &&
        error "log with invalid fsevents count should have failed"

    rbh_log "rbh:$db:$testdb" --fsevents 42invalid &&
        error "log with invalid fsevents count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-fsevents \
        "rbh-fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb"

    echo "$output" | grep "Enrichment" > /dev/null ||
        error "enrich_mountpoint should have been retrieved"

    echo "$output" | grep "Source" > /dev/null ||
        error "source_read should have been retrieved"

    echo "$output" | grep "Number" > /dev/null ||
        error "worker_count should have been retrieved"
}

test_N_logs()
{
    local requested=$1
    local expected=$2

    touch blob
    for i in $(seq 1 $expected); do
        invoke_rbh-fsevents
        sleep 1
    done

    local output=$(rbh_log "rbh:$db:$testdb" --fsevents $requested)

    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 9 * $expected)); then
        error "There should be 9 * $expected lines about fsevents (7 for content, 2 for header/footer, time $expected logs), got '$output'"
    fi

    for i in $(seq 1 $expected); do
        local one_log="$(echo "$output" | head -n 9)"
        check_log_result "$one_log"
        output="$(echo "$output" | sed 1,9d)"
    done
}

test_fsevents_1()
{
    test_N_logs 1 1
}

test_fsevents_N()
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

declare -a tests=(test_invalid test_fsevents_1 test_fsevents_N test_more_than_N)

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
