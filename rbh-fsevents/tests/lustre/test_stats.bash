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
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" --stats --log-timer blob &&
        error "Fsevents with invalid log timer should have failed"

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" --stats --log-timer -3 &&
        error "Fsevents with invalid log timer should have failed"

    return 0
}

test_stats()
{
    touch test_entry

    local output="$(rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" --stats | tail -n 12)"

    echo "$output" | grep "rbh-fsevents" > /dev/null ||
        error "Invalid output, missing 'rbh-fsevents' command, got '$output'"
    echo "$output" | grep "changelog read" | grep "2" > /dev/null ||
        error "Invalid changelog read count, got '$output', expected '2'"
    echo "$output" | grep "worker" | grep "1" > /dev/null ||
        error "Invalid worker count, got '$output', expected '1'"
    echo "$output" | grep "changelog/sec" | wc -l | grep "3" > /dev/null ||
        error "Invalid output, missing 3 'changelog/sec' lines, got '$output'"

    mkdir blob
    rm test_entry
    ln -s something blob
    touch else
    lfs setstripe -E -1 -c 2 -S 256k file
    lfs mkdir dir

    output="$(rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" --stats --nb-workers 4 |
        tail -n 12)"
    local count="$(lfs changelog $LUSTRE_MDT $userid | wc -l)"

    echo "$output" | grep "rbh-fsevents" > /dev/null ||
        error "Invalid output, missing 'rbh-fsevents' command, got '$output'"
    echo "$output" | grep "changelog read" | grep "$count" > /dev/null ||
        error "Invalid changelog read count, got '$output', expected '$count'"
    echo "$output" | grep "worker" | grep "4" > /dev/null ||
        error "Invalid worker count, got '$output', expected '4'"
    echo "$output" | grep "changelog/sec" | wc -l | grep "3" > /dev/null ||
        error "Invalid output, missing 3 'changelog/sec' lines, got '$output'"

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" src:lustre:"$LUSTRE_MDT" \
        "rbh:$db:$testdb" --stats --nb-workers 4 --log-file logs.txt
    count="$(lfs changelog $LUSTRE_MDT $userid | wc -l)"
    output="$(cat logs.txt | tail -n 12)"

    echo "$output" | grep "rbh-fsevents" > /dev/null ||
        error "Invalid output, missing 'rbh-fsevents' command, got '$output'"
    # $count - 1 because the file logs.txt is created by the command, thus it
    # adds an CREAT changelog to be read, but it is closed after everything is
    # read, so the corresponding CLOSE changelog is not handled by rbh-fsevents,
    # so the command only reads $count - 1 changelogs.
    echo "$output" | grep "changelog read" |
        grep "$((count - 1))" > /dev/null ||
        error "Invalid changelog read count, got '$output', expected '$((count - 1))'"
    echo "$output" | grep "worker" | grep "4" > /dev/null ||
        error "Invalid worker count, got '$output', expected '4'"
    echo "$output" | grep "changelog/sec" | wc -l | grep "3" > /dev/null ||
        error "Invalid output, missing 3 'changelog/sec' lines, got '$output'"
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
