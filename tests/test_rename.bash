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

test_rename()
{
    local entry="$1"
    local entry_renamed="$2"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    find_attribute '"ns": { $exists : true }' '"ns": { $size : 1 }' \
                   '"ns.name":"'$entry_renamed'"'

    # Request the DB to only show the inode with name $entry_renamed, and return
    # its parent
    local entry_parent=$(mongo "$testdb" --eval \
                         'db.entries.find({"ns.name":"'$entry_renamed'"},
                                          {"ns.parent": 1, "_id": 0})')

    # The parent is of the form
    # '{ "ns" : [ { "parent" : BinData(0,"lw...AA") } ] }'
    # so we do some bashism to remove the start until 'BinData' and then after
    # the first closing parenthesis
    entry_parent="BinData${entry_parent#*BinData}"
    entry_parent="${entry_parent%)*})"

    local parent=$(mongo "$testdb" --eval \
                   'db.entries.find({"_id":'$entry_parent'}, {"_id": 1})')

    if [[ "$parent" != *"$entry_parent"* ]]; then
        error "'$entry_renamed' is not located in the '$parent' directory, but"
              "in '$entry_parent'"
    fi
}

test_rename_same_dir()
{
    local entry="test_entry"
    local entry_renamed="test_entry_renamed"
    touch $entry
    mv $entry $entry_renamed

    test_rename "$entry" "$entry_renamed"
}

test_rename_different_dir()
{
    local entry="test_entry"
    local entry_renamed="test_entry_renamed"
    mkdir tmp
    touch $entry
    mv $entry tmp/$entry_renamed

    test_rename "$entry" "$entry_renamed"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_rename_same_dir test_rename_different_dir)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
