#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

get_binary_id_from_mongo_entry()
{
    local entry="$1"

    # We do some bashism to only retrieve the ID part, which for Lustre starts
    # with "AQ" and ends with "AA="
    entry="$(echo "$entry" | tr -d '\n')"
    entry="AQ${entry#*AQ}"
    entry="${entry%AA=*}AA="

    echo "$entry"
}

get_mongo_id_from_binary_id()
{
    local binary="$1"

    if (( $(mongo_version) < $(version_code 5.0.0) )); then
        echo "BinData(0, \"$binary\")"
    else
        echo "Binary.createFromBase64(\"$binary\", 0)"
    fi
}

get_entry_from_binary_id()
{
    local id="$1"

    mongo "$testdb" --eval \
        "db.entries.find({\"_id\":$(get_mongo_id_from_binary_id "$id")},
                         {\"_id\": 1})"
}

test_rename()
{
    local entry="$1"
    local entry_renamed="$2"
    local count=$3

    invoke_rbh-fsevents

    local entries=$(count_documents)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    find_attribute '"ns": { $exists : true }' '"ns": { $size : 1 }' \
                   '"ns.name":"'$entry_renamed'"'

    # Request the DB to only show the inode with name $entry_renamed, and return
    # its parent
    local entry_parent="$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry_renamed'"},
                         {"ns.parent": 1, "_id": 0})')"

    local parent_id="$(get_binary_id_from_mongo_entry "$entry_parent")"

    local parent="$(get_entry_from_binary_id "$parent_id")"

    if [[ "$parent" != *"$parent_id"* ]]; then
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

    archive_file tmp/$entry_renamed
    clear_changelogs "$LUSTRE_MDT" "$userid"

    # Retrieve the overwriten link's id to test the link was deleted but the
    # inode still exists since an archived copy is still valid
    local old_entry=$(mongo "$testdb" --eval \
                      'db.entries.find({"ns.name":"'$entry_renamed'"},
                                       {"_id": 1})')

    old_entry="$(get_binary_id_from_mongo_entry "$old_entry")"

    mv $entry tmp/$entry_renamed
    local count=$(find . | wc -l)
    count=$((count + 1))

    test_rename $entry $entry_renamed $count

    # Check the overwriten entry is still in the DB but has no link anymore
    local result="$(mongo "$testdb" --eval \
        "db.entries.find({\"_id\":$(get_mongo_id_from_binary_id "$old_entry"),
                          \"ns\": { \$exists : true },
                          \"ns\": { \$size : 0 }})")"
    if [ -z "$result" ]; then
        error "Entry '$old_entry' should still be in the DB, but with no link"
    fi

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
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
