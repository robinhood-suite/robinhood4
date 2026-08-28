#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

test_stats()
{
    touch fileA
    ln fileA fileB
    touch fileC

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rm fileA

    rbh_gc "rbh:$db:$testdb" --dry-run --stats |
        difflines "'/fileA' needs to be deleted" \
                  "1 element total to delete"

    rm fileB

    local output="$(rbh_gc "rbh:$db:$testdb" --stats)"

    echo "$output" | grep "rbh-gc" > /dev/null ||
        error "Invalid command in stats, got '$output', expected 'rbh-gc'"
    echo "$output" | grep "progress" | grep "2 entries deleted" > /dev/null ||
        error "Invalid progress stats, got '$output', expected '2 entries deleted'"
    echo "$output" | grep "progress" | grep "2 kept" > /dev/null ||
        error "Invalid progress stats, got '$output', expected '2 kept'"
}

declare -a tests=(test_stats)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
