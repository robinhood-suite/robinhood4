#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_atime()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%a\n")
    local d=$(date -d "$date")
    local atime=$(date -d "@$(stat -c %X file)")

    if [[ "$d" != "$atime" ]]; then
        error "wrong atime: '$d' != '$atime'"
    fi
}

test_ctime()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%c\n")
    local d=$(date -d "$date")
    local ctime=$(date -d "@$(stat -c %Z file)")

    if [[ "$d" != "$ctime" ]]; then
        error "wrong ctime: '$d' != '$ctime'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_atime test_ctime)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
