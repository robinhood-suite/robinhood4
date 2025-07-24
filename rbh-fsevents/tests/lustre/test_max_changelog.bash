#!/usr/bin/env bash

# This file is part of rbh-fsevents
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

test_max_changelog()
{
    local entry1="test_entry1"
    local entry2="test_entry2"
    touch "$entry1"
    touch "$entry2"

    local nb_changelog=$(lfs changelog $LUSTRE_MDT $userid | wc -l)

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --max 2 \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"

    local nb_changelog_after=$(lfs changelog $LUSTRE_MDT $userid | wc -l)
    if [[ $(($nb_changelog - 2)) -ne $nb_changelog_after ]]; then
        error "There should be only $(($nb_changelog - 2)) changelogs"
              "remaining to read, got $nb_changelog_after"
    fi

    local entries=$(count_documents)
    if [[ $entries != 2 ]]; then
        error "There should be only 2 entries in the database"
    fi

    verify_statx "$entry1"
    verify_lustre "$entry1"

    find_attribute '"xattrs.nb_children": 1'

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --max 8 \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"

    nb_changelog=$(lfs changelog $LUSTRE_MDT $userid | wc -l)
    if [[ $nb_changelog -ne 0 ]]; then
        error "There should be no changelogs remaining to read"
    fi

    entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries != $count ]]; then
        error "There should be only $count entries in the database"
    fi

    verify_statx "$entry2"
    verify_lustre "$entry2"

    find_attribute '"xattrs.nb_children": 2'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_max_changelog)

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
