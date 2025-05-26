#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_verbose()
{
    touch file
    mkdir dir

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_find --verbose "rbh:$db:$testdb" -type f)"

    echo "$output" | grep "/file"
    # Value "32768" taken from unistd.h
    echo "$output" | grep "\$match" | grep "statx.type" | grep "\$eq" |
                     grep "32768"

    local output="$(rbh_find --verbose "rbh:$db:$testdb" -type d)"

    echo "$output" | grep "/dir"
    # Value "16384" taken from unistd.h
    echo "$output" | grep "\$match" | grep "statx.type" | grep "\$eq" |
                     grep "16384"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_verbose)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
