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
    local count=$3

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
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

    local count=$(find . | wc -l)
    test_rename $entry $entry_renamed $count

    verify_lustre $entry_renamed
}

test_rename_different_dir()
{
    local entry="test_entry"
    local entry_renamed="test_entry_renamed"
    mkdir tmp
    touch $entry
    mv $entry tmp/$entry_renamed

    local count=$(find . | wc -l)
    test_rename $entry $entry_renamed $count

    verify_lustre tmp/$entry_renamed
}

test_rename_overwrite_data()
{
    local entry="test_entry"
    local entry_renamed="test_entry_renamed"
    touch $entry
    mkdir tmp
    touch tmp/$entry_renamed

    mv $entry tmp/$entry_renamed
    local count=$(find . | wc -l)

    test_rename $entry $entry_renamed $count

    verify_lustre tmp/$entry_renamed
}

test_rename_overwrite_data_with_hsm_copy()
{
    local entry="test_entry"
    local entry_renamed="test_entry_renamed"
    touch $entry
    mkdir tmp
    touch tmp/$entry_renamed

    invoke_rbh-fsevents

    hsm_archive_file tmp/$entry_renamed
    clear_changelogs "$LUSTRE_MDT" "$userid"

    # Retrieve the overwriten link's id to test the link was deleted but the
    # inode still exists since an archived copy is still valid
    local old_entry=$(mongo "$testdb" --eval \
                      'db.entries.find({"ns.name":"'$entry_renamed'"},
                                       {"_id": 1})')

    old_entry="BinData${old_entry#*BinData}"
    old_entry="${old_entry%)*})"

    mv $entry tmp/$entry_renamed
    local count=$(find . | wc -l)
    count=$((count + 1))

    test_rename $entry $entry_renamed $count

    # Check the overwriten entry is still in the DB but has no link anymore
    find_attribute '"_id":'$old_entry \
                   '"ns": { $exists : true }' '"ns": { $size : 0 }'
    verify_lustre tmp/$entry_renamed
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_rename_same_dir test_rename_different_dir
                  test_rename_overwrite_data)

if lctl get_param mdt.*.hsm_control | grep "enabled"; then
    tests+=(test_rename_overwrite_data_with_hsm_copy)
fi

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; clear_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
