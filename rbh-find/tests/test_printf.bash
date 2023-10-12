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
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%a\n")
    local d=$(date -d "$date")
    local atime=$(date -d "@$(stat -c %X file)")

    if [[ "$d" != "$atime" ]]; then
        error "printf atime: '$d' != actual '$atime'"
    fi
}

test_ctime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%c\n")
    local d=$(date -d "$date")
    local ctime=$(date -d "@$(stat -c %Z file)")

    if [[ "$d" != "$ctime" ]]; then
        error "printf ctime: '$d' != actual '$ctime'"
    fi
}

test_mtime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%t\n")
    local d=$(date -d "$date")
    local mtime=$(date -d "@$(stat -c %Y file)")

    if [[ "$d" != "$mtime" ]]; then
        error "printf mtime: '$d' != actual '$mtime'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_atime test_ctime test_mtime)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
