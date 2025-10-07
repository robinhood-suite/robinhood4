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

test_save_idx()
{
    local entry="test_entry"

    touch $entry

    local last_changelog=$(lfs changelog $LUSTRE_MDT | tail -n 1 |
                           cut -d' ' -f1)

    invoke_rbh-fsevents

    local fsevents_source_idx=$(do_db fsevents "$testdb" "$LUSTRE_MDT")

    if [[ $fsevents_source_idx != $last_changelog ]]; then
        error "Fsevents source index should be $nb_changelog"
    fi
}

test_save_idx_multiple_mdt()
{
    local entry="test_entry"
    local userid2=$(start_changelogs lustre-MDT0001)

    lfs mkdir --mdt-index 0 "mdt0"
    touch "mdt0/$entry"

    lfs mkdir --mdt-index 1 "mdt1"
    touch "mdt1/$entry"

    local last_changelog=$(lfs changelog $LUSTRE_MDT | tail -n 1 |
                           cut -d' ' -f1)
    local last_changelog2=$(lfs changelog lustre-MDT0001 | tail -n 1 |
                           cut -d' ' -f1)

    invoke_rbh-fsevents
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:lustre-MDT0001?ack-user="$userid2" "rbh:$db:$testdb"

    stop_changelogs lustre-MDT0001 $userid2

    local fsevents_source_idx=$(do_db fsevents "$testdb" "$LUSTRE_MDT")
    local fsevents_source_idx2=$(do_db fsevents "$testdb" "lustre-MDT0001")

    if [[ $fsevents_source_idx != $last_changelog ]]; then
        error "Fsevents source index for $LUSTRE_MDT should be $last_changelog"
    fi

    if [[ $fsevents_source_idx2 != $last_changelog2 ]]; then
        error "Fsevents source index for lustre-MDT0001 " \
              "should be $last_changelog2"
    fi
}

test_start_idx()
{
    local entry="test_entry"
    local entry2="test_entry2"
    local userid2=$(start_changelogs $LUSTRE_MDT)

    touch $entry

    local last_changelog=$(lfs changelog $LUSTRE_MDT | tail -n 1 |
                           cut -d' ' -f1)

    invoke_rbh-fsevents

    local fsevents_source_idx=$(do_db fsevents "$testdb" "$LUSTRE_MDT")
    if [[ $fsevents_source_idx != $last_changelog ]]; then
        stop_changelogs $LUSTRE_MDT $userid2
        error "Fsevents source index for $LUSTRE_MDT should be $last_changelog"
    fi

    # Add another changelog, with 2 users and since only the first has done
    # the acknowledgment, all the changelog are still available.
    # rbh-fsevents should only read the changelog for test_entry2
    touch $entry2

    # Clear all the entries, after rbh-fsevents, we should only have test_entry2
    # in the database
    do_db clear_entries "$testdb"

    last_changelog=$(lfs changelog $LUSTRE_MDT | tail -n 1 | cut -d' ' -f1)

    invoke_rbh-fsevents

    stop_changelogs $LUSTRE_MDT $userid2

    fsevents_source_idx=$(do_db fsevents "$testdb" "$LUSTRE_MDT")
    if [[ $fsevents_source_idx != $last_changelog ]]; then
        error "Fsevents source index for $LUSTRE_MDT should be $last_changelog"
    fi

    local path="$PWD"
    path="${path#/mnt/lustre}"
    rbh_find rbh:$db:$testdb | sort | difflines "$path/test_entry2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_save_idx test_save_idx_multiple_mdt test_start_idx)

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
