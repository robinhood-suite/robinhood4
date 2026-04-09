#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

test_hardlink()
{
    touch fileA
    ln fileA fileB

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rm fileA

    rbh_gc "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileB"

    rm fileB

    rbh_gc "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | difflines "/"
}

declare -a tests=(test_hardlink)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
