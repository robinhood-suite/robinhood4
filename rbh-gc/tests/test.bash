#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash


test_basic()
{
    touch fileA fileB

    pwd
    ls

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileA" "/fileB"

    rbh_gc "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileA" "/fileB"

    rm fileA

    rbh_gc -d -v "rbh:$db:$testdb"
    rbh_gc "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileB"
}


declare -a tests=(test_basic)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

pwd

run_tests ${tests[@]}
