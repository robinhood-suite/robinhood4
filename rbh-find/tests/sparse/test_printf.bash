#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_sparseness()
{
    local file0="non_sparse"
    local file1="sparse"

    touch $file0 $file1
    bs0=$(stat -c "%o" "$file0")
    bs1=$(stat -c "%o" "$file1")

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file0" \
        bs=$bs0 count=4

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file1" \
        bs=$bs1 count=1
    dd oflag=direct conv=notrunc if=/dev/urandom of="$file1" \
        bs=$bs1 count=1 seek=2

    rbh_sync "rbh:sparse:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RSS'\n" | sort |
        difflines "/: 'None'" "/$file0: '1.00'" "/$file1: '0.66'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sparseness)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
