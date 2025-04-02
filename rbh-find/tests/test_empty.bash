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

test_empty()
{
    mkdir dir1 dir2

    touch empty dir2/fileA
    truncate --size 1K fileB
    ln -s empty empty_link

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -empty | sort |
        difflines "/dir1" "/dir2/fileA" "/empty"
    rbh_find "rbh:mongo:$testdb" -not -empty | sort |
        difflines "/" "/dir2" "/empty_link" "/fileB"
    rbh_find "rbh:mongo:$testdb" -not -empty -or -empty | sort |
        difflines "/" "/dir1" "/dir2" "/dir2/fileA" "/empty" "/empty_link" \
                  "/fileB"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_empty)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
