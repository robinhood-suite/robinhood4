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

    local list=$(rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$PWD" --list)
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

declare -a tests=(test_list)

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
