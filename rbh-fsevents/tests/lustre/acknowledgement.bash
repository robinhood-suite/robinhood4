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

acceptance()
{
    local dir="test_dir"
    local file1="test_file1"
    local file2="test_file2"

    touch $file1
    mkdir $dir
    touch $dir/$file2

    invoke_rbh-fsevents
    local changelogs=$(lfs changelog $LUSTRE_MDT)

    if [[ ! -z "$changelogs" ]]; then
        error "Changelogs corresponding to the touch/mkdir should have " \
              "been cleared"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(acceptance)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests lustre_setup lustre_teardown "${tests[@]}"
