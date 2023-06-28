#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

create_entry()
{
    touch "$1"
}

rm_entry()
{
    rm -f "$1"
}

test_rm_with_hsm_copy()
{
    local entry="test_entry"
    create_entry $entry

    invoke_rbh-fsevents

    hsm_archive_file $entry
    clear_changelogs "$LUSTRE_MDT" "$userid"
    rm_entry $entry

    invoke_rbh-fsevents

    # Since an archived copy of $entry still exists, the DB should still contain
    # $entry with no parent
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    count=$((count + 1))
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    find_attribute '"ns": { $exists : true }' '"ns": { $size : 0 }'
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_rm_inode.bash

declare -a tests=(test_rm_same_batch test_rm_different_batch)

if lctl get_param mdt.*.hsm_control | grep "enabled"; then
    tests+=(test_rm_with_hsm_copy)
fi

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
