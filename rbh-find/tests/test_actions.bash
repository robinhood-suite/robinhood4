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

test_exec()
{
    echo "test data" > file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    # XXX for now, rbh-find -exec only works at the root of the filesystem
    # We don't know where the filesystem coresponding to the data base is stored
    # A possible solution would be to store the FS name when creating the
    # database
    rbh_find "rbh:mongo:$testdb" -exec grep -H "test data" {} ";" | sort |
        difflines "file:test data"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_exec)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
