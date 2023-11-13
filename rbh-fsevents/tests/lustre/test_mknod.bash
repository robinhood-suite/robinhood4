#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../test_utils.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

create_entry()
{
    mknod "$1" b 1 2
}

create_filled_entry()
{
    mknod "$1" b 1 2
}

test_create_mknod()
{
    local entry="test_entry"
    mknod $entry.1 b 1 2
    mknod $entry.2 p

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    verify_statx "$entry.1"
    verify_lustre "$entry.1"
    verify_statx "$entry.2"
    verify_lustre "$entry.2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_create_mknod test_create_two_entries)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests lustre_setup lustre_teardown "${tests[@]}"
