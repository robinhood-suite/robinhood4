#!/usr/bin/env bash

# This file is part of rbh-undelete.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/../../rbh-fsevents/tests/lustre/lustre_utils.bash

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

    hsm_restore_file $entry

    if [ ! -f "$entry" ]; then
        error "rbh-undelete failed to restore $entry"
    fi

    diff "$entry" "$cp_entry" || error "Content restored is not matching"

    rm $entry $cp_entry
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

    hsm_restore_file "$output"

    if [ ! -f "$output" ]; then
        error "rbh-undelete failed to restore $output"
    fi

    diff "$output" "$cp_entry" || error "Content restored is not matching"

    rm -f "$entry" "$cp_entry" "$output"
    rmdir "$test_dir"
}

test_list()
{
    local fileA="test"
    local fileB="test2"

    touch $fileB
    touch $fileA

    local path=$(realpath "$fileA")

    archive_file $fileA

    invoke_rbh-fsevents

    rm $fileA

    invoke_rbh-fsevents

    local list=$(rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$pwd" --list)
    local rm_path=$(echo "$list" | grep rm_path |
        sed -n 's/.*rm_path: \([^ ]*\).*/\1/p')
    local n_lines=$(echo "$list" | wc -l)

    if ((n_lines != 2)); then
        error "There shoud be only one file deleted"
    fi

    local mountpoint=$(mongo "$testdb" --eval \
    'db.info.findOne({_id: "mountpoint_info"}, {mountpoint: 1}).mountpoint')

    local rm_full_path="${mountpoint}${rm_path}"

    if [[ "$rm_full_path" != "$path" ]]; then
        error "rm_path is not equal to path"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_undelete_with_output)

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
