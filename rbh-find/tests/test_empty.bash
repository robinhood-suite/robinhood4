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

test_empty_file()
{
    touch empty
    truncate --size 1K file
    ln -s empty empty_link

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -empty | sort | difflines "/empty"
    rbh_find "rbh:$db:$testdb" -not -empty | sort |
        difflines "/" "/empty_link" "/file"
    rbh_find "rbh:$db:$testdb" -not -empty -or -empty | sort |
        difflines "/" "/empty" "/empty_link" "/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_empty_file)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
