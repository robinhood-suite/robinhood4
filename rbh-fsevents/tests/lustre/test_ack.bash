#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_ack()
{
    touch entry
    touch entry2
    touch entry3
    rm entry2

    # Each touch generates two changelogs (one CREATE and one CLOSE), and the rm
    # generates one changelog (UNLINK). In total, we have 7 changelogs. A CREATE
    # changelog generates fsevents for the entry and its parent. With a batch
    # size of 2, the first CREATE and CLOSE are read, filling the batch. This
    # batch is then enriched, updated, and acknowledged. Next, a batch is
    # created for the second touch, but enrichment fails because entry2 has been
    # removed.
    rbh_fsevents -w 1 -b 2 --no-skip -e rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb" &&
        error "rbh-fsevents should have failed"

    local count=$(lfs changelog $LUSTRE_MDT | wc -l)
    if [[ $count -ne 5 ]]; then
        error "There should be 5 changelogs left, got $count"
    fi
}

test_ack_no_dedup()
{
    touch entry
    touch entry2
    rm entry2

    rbh_fsevents -w 1 -b 0 --no-skip -e rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb" &&
        error "rbh-fsevents should have failed"

    local count=$(lfs changelog $LUSTRE_MDT | wc -l)
    if [[ $count -ne 3 ]]; then
        error "There should be 3 changelogs left, got $count"
    fi
}

test_ack_thread()
{
    touch entry
    touch entry2
    touch entry3
    rm entry2

    rbh_fsevents -w 2 -b 2 --no-skip -e rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb" &&
        error "rbh-fsevents should have failed"

    local count=$(lfs changelog $LUSTRE_MDT | wc -l)
    if [[ $count -ne 5 ]]; then
        error "There should be 5 changelogs left, got $count"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_ack test_ack_no_dedup test_ack_thread)

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
