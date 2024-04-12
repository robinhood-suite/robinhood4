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
    # test_create_two_entries and test_create_entry_check_statx_attr will only
    # check for the symlink itself, and not other entries, so creating two
    # entries here is not an issue
    touch "$1.tmp"
    ln -s "$1.tmp" "$1"
}

create_filled_entry()
{
    echo "test" > "$1.tmp"
    ln -s "$1.tmp" "$1"
}

test_create_symlink()
{
    local entry="test_entry"
    create_entry "$entry"

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    verify_statx "$entry"
    verify_statx "$entry.tmp"
    find_attribute "\"ns.name\":\"$entry\"" "\"symlink\":\"$entry.tmp\""
}

test_symlink_other_mdt()
{
    local entry="test_entry"
    local dir="test_dir"

    old_LUSTRE_MDT=$LUSTRE_MDT
    old_userid=$userid

    LUSTRE_MDT=lustre-MDT0003
    userid="$(start_changelogs "$LUSTRE_MDT")"

    touch $entry
    lfs mkdir -i 3 $dir
    ln -s $entry $dir/$entry.tmp

    invoke_rbh-fsevents

    clear_changelogs $LUSTRE_MDT $userid
    stop_changelogs $LUSTRE_MDT $userid

    LUSTRE_MDT=$old_LUSTRE_MDT
    userid=$old_userid

    find_attribute "\"ns.name\":\"$entry.tmp\"" "\"symlink\":\"$entry\""
    find_attribute "\"ns.name\":\"$entry.tmp\"" "\"xattrs.mdt_index\":3"
}

################################################################################
#                                     MAIN                                     #
################################################################################

source $test_dir/test_create_inode.bash

declare -a tests=(test_create_symlink test_create_two_entries
                  test_symlink_other_mdt)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests lustre_setup lustre_teardown "${tests[@]}"
