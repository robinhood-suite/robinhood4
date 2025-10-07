#!/usr/bin/env bash

# This file is part of rbh-undelete.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/../../rbh-fsevents/tests/lustre/lustre_utils.bash

check_undeleted_file_match()
{
    local entry="$1"
    local cp_entry="$1"

    if [ ! -f "$entry" ]; then
        error "rbh-undelete failed to restore $entry"
    fi

    hsm_restore_file $entry

    diff "$entry" "$cp_entry" || error "Content restored is not matching"
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_simple_undelete()
{
    local entry="entry"
    local cp_entry="cp_entry"

    echo "test_content" > "$entry"

    local path=$(realpath "$entry")

    cp $entry $cp_entry

    archive_file $entry

    invoke_rbh-fsevents

    rm $entry

    invoke_rbh-fsevents

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --restore

    check_undeleted_file_match "$entry" "$cp_entry"
}

test_double_undelete()
{
    local entry="entry"
    local cp_entry="cp_entry"

    echo "test_content" > "$entry"

    local path=$(realpath "$entry")

    cp $entry $cp_entry

    archive_file $entry

    invoke_rbh-fsevents

    rm $entry

    invoke_rbh-fsevents

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --restore

    check_undeleted_file_match "$path" "$cp_entry"

    invoke_rbh-fsevents

    rm "$entry"
    invoke_rbh-fsevents

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --restore

    check_undeleted_file_match "$path" "$cp_entry"
}

test_undelete_with_output()
{
    local entry="entry"
    local cp_entry="cp_entry"
    local test_dir="test_dir"

    echo "test_content" > "$entry"
    local path=$(realpath "$entry")

    mkdir -p $test_dir

    cp "$entry" "$cp_entry"

    archive_file "$entry"
    invoke_rbh-fsevents

    rm "$entry"
    invoke_rbh-fsevents

    local output="$(pwd)/$test_dir/output_entry"

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --output "$output" --restore

    check_undeleted_file_match "$output" "$cp_entry"
}

test_undelete_with_system_mountpoint()
{
    local entry="entry"
    local cp_entry="cp_entry"

    echo "test_content" > "$entry"
    local path=$(realpath "$entry")

    cp "$entry" "$cp_entry"

    archive_file "$entry"
    invoke_rbh-fsevents_no_ack

    rm "$entry"
    invoke_rbh-fsevents_no_ack

    mongo $testdb --eval "db.info.find()"
    do_db drop_info "$testdb"

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --restore &&
        error "Undelete with missing mountpoint and absolute path should have failed"

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$entry" --restore &&
        error "Undelete with missing mountpoint and relative path should have failed"

    rbh_sync rbh:lustre:/mnt/lustre rbh:$db:$testdb
    do_db drop_info "$testdb"

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --restore ||
        error "Undelete with absolute path and known current path should have succeeded"

    check_undeleted_file_match "$entry" "$cp_entry"

    rbh_sync rbh:lustre:/mnt/lustre rbh:$db:$testdb
    rm "$entry"

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$entry" --restore ||
        error "Undelete with relative path and known current path should have succeeded"

    check_undeleted_file_match "$entry" "$cp_entry"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_simple_undelete test_double_undelete
                  test_undelete_with_output
                  test_undelete_with_system_mountpoint)

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
