#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_sparse()
{
    local file0="non_sparse"
    local file1="sparse"
    local file2="empty"

    touch $file0 $file1 $file2
    bs0=$(stat -c "%o" "$file0")
    bs1=$(stat -c "%o" "$file1")

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file0" \
        bs=$bs0 count=4

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file1" \
        bs=$bs1 count=1

    invoke_rbh-fsevents

    find_attribute "\"ns.name\": \"$file0\"" "\"xattrs.sparseness\": 100"
    find_attribute "\"ns.name\": \"$file1\"" "\"xattrs.sparseness\": 100"
    find_attribute "\"ns.name\": \"$file2\"" "\"xattrs.sparseness\": 100"

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file1" \
        bs=$bs1 count=1 seek=2

    invoke_rbh-fsevents

    local sparse1=$(bc <<< "$(find . -name "$file1" -printf "%S") * 100")
    sparse1="${sparse1%.*}"

    find_attribute "\"ns.name\": \"$file0\"" "\"xattrs.sparseness\": 100"
    find_attribute "\"ns.name\": \"$file1\"" "\"xattrs.sparseness\": $sparse1"
    find_attribute "\"ns.name\": \"$file2\"" "\"xattrs.sparseness\": 100"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sparse)

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
