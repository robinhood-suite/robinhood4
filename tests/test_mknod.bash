#!/usr/bin/env bash

# This file is part of rbh-fsevents
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
        error "There should be only $count entries in the database"
    fi

    local raw_mode="$(statx +%f "$entry.1" 16)"
    local type=$((raw_mode & 00170000))
    find_attribute "\"ns.name\":\"$entry.1\"" "\"statx.type\":$type" \
                   "\"statx.rdev.major\":1" "\"statx.rdev.minor\":2"

    raw_mode="$(statx +%f "$entry.2" 16)"
    type=$((raw_mode & 00170000))
    find_attribute "\"ns.name\":\"$entry.2\"" "\"statx.type\":$type"

    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry.1\""
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry.2\""
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry.3\""
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_create_mknod test_create_two_entries
                  test_create_entry_check_statx_attr)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
