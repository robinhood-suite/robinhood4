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

test_unlink_same_batch()
{
    local entry="test_entry"
    touch $entry
    rm -f $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    # The only entry in the database should be the mount point, as the created
    # entry was immediately deleted and should have been ignored
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi
}

test_unlink_different_batch()
{
    local entry="test_entry"
    touch $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    setfattr -n user.blob -v 42 $entry
    rm -f $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi
}

test_unlink_with_hsm_copy()
{
    local entry="test_entry"
    touch $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    archive_file $entry
    clear_changelogs
    rm -f $entry

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    # Since an archived copy of $entry still exists, the DB should contain two
    # entries
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    mongo "$testdb" --eval "db.entries.find()"
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

declare -a tests=(test_unlink_same_batch test_unlink_different_batch)

if lctl get_param mdt.*.hsm_control | grep "enabled"; then
    tests+=(test_unlink_with_hsm_copy)
fi

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
