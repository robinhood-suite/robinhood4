#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_blocks()
{
    touch file1 file2 file3

    dd oflag=direct if=/dev/urandom of=file2 bs=1M count=1
    dd oflag=direct if=/dev/urandom of=file3 bs=2M count=1

    local blocks1="$(stat -c %b file1)"
    local blocks2="$(stat -c %b file2)"
    local blocks3="$(stat -c %b file3)"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -blocks $blocks1 | sort |
        difflines "/" "/file1"
    rbh_find "rbh:mongo:$testdb" -blocks +$blocks1 | sort |
        difflines "/file2" "/file3"
    rbh_find "rbh:mongo:$testdb" -blocks -$blocks3 | sort |
        difflines "/" "/file1" "/file2"

    rbh_find "rbh:mongo:$testdb" -blocks $blocks2 -o -blocks +$blocks2 | sort |
        difflines "/file2" "/file3"
    rbh_find "rbh:mongo:$testdb" -blocks +$blocks2 -a -blocks -$blocks2 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" -not -blocks $blocks2 | sort |
        difflines "/" "/file1" "/file3"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_blocks)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
