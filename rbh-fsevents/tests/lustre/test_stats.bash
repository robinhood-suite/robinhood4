#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid_stat_options()
{
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --stats --stats-interval blob &&
        error "Fsevents with invalid log interval should have failed"

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --stats --stats-interval -3 &&
        error "Fsevents with invalid log interval should have failed"

    return 0
}

find_expected_values()
{
    local output="$1"
    local changelog_read="$2"
    local worker_count="$3"

    echo "$output" | grep "rbh-fsevents" > /dev/null ||
        error "Invalid output, missing 'rbh-fsevents' command, got '$output'"
    echo "$output" | grep "changelog read" |
        grep "$changelog_read" > /dev/null ||
        error "Invalid changelog read count, got '$output', expected '$changelog_read'"
    echo "$output" | grep "worker" | grep "$worker_count" > /dev/null ||
        error "Invalid worker count, got '$output', expected '$worker_count'"
    echo "$output" | grep "sec/changelog" | wc -l | grep "1" > /dev/null ||
        error "Invalid output, missing 1 'sec/changelog' line, got '$output'"
    echo "$output" | grep "sec/worker/changelog" | wc -l |
        grep "1" > /dev/null ||
        error "Invalid output, missing 1 'sec/worker/changelog' line, got '$output'"
    echo "$output" | grep "changelog/sec" | wc -l | grep "1" > /dev/null ||
        error "Invalid output, missing 1 'changelog/sec' line, got '$output'"
}

test_stats()
{
    touch test_entry

    local output="$(rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" --stats 2>&1 | tail -n 12)"
    find_expected_values "$output" 2 1

    mkdir blob
    rm test_entry
    ln -s something blob
    touch else
    lfs setstripe -E -1 -c 2 -S 256k file
    lfs mkdir dir

    output="$(rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" --stats --nb-workers 4 2>&1 |
        tail -n 12)"
    local count="$(lfs changelog $LUSTRE_MDT $userid | wc -l)"
    find_expected_values "$output" "$count" 4

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" src:lustre:"$LUSTRE_MDT" \
        "rbh:$db:$testdb" --stats --nb-workers 4 --stats-file logs.txt
    count="$(lfs changelog $LUSTRE_MDT $userid | wc -l)"
    output="$(cat logs.txt | tail -n 12)"
    # $count - 1 because the file logs.txt is created by the command, thus it
    # adds an CREAT changelog to be read, but it is closed after everything is
    # read, so the corresponding CLOSE changelog is not handled by rbh-fsevents,
    # so the command only reads $count - 1 changelogs.
    find_expected_values "$output" "$((count - 1))" 4

    output="$(rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
                src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
                --stats --nb-workers 4 --stats-file syslog 2>&1)"
    # Remove the normal stderr lines associated with the exec failures, the
    # actual stdout lines of the exec success, and then the 'RobinHood4[<pid>]'
    # header syslog adds.
    output="$(echo "${output#*: }")"
    local syslog_output="$(journalctl -t RobinHood4 -o cat)"

    journalctl -t RobinHood4 --rotate
    journalctl -t RobinHood4 --vacuum-time=10s

    find_expected_values "$output" "$count" 4
    if [ "$output" != "$syslog_output" ]; then
        error "Syslog output and stderr should have been the same," \
              "got syslog '$syslog_output' and stderr '$output'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid_stat_options test_stats)

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
