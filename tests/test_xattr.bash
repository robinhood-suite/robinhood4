#!/usr/bin/env bash

# This file is part of rbh-find.
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -e

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

################################################################################
#                                  UTILITIES                                   #
################################################################################

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

__rbh_find=$(PATH="$PWD:$PATH" which rbh-find)
rbh_find()
{
    "$__rbh_find" "$@"
}

__mongo=$(which mongosh || which mongo)
mongo()
{
    "$__mongo" "$@"
}

setup()
{
    # Create a test directory and `cd` into it
    testdir=$SUITE-$test
    mkdir "$testdir"
    cd "$testdir"

    # "Create" a test database
    testdb=$SUITE-$test
}

teardown()
{
    mongo --quiet "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
}

difflines()
{
    diff -y - <([ $# -eq 0 ] && printf '' || printf '%s\n' "$@")
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_xattr_exists_no_arg()
{
    touch "file"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    ! rbh_find "rbh:mongo:$testdb" -xattr
}

test_xattr_exists()
{
    touch "file"
    setfattr --name user.key --value val file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -xattr user | sort |
        difflines "/file"
    rbh_find "rbh:mongo:$testdb" -xattr user.key | sort |
        difflines "/file"
}

test_xattr_not_exists()
{
    touch "file"
    setfattr --name user.key --value val file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -xattr blob | sort | difflines
    rbh_find "rbh:mongo:$testdb" -xattr user.err | sort | difflines
}

test_not_xattr_exists()
{
    touch "file"
    setfattr --name user.key --value val file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -not -xattr user | sort |
        difflines "/"
    rbh_find "rbh:mongo:$testdb" -not -xattr user.key | sort |
        difflines "/"
    rbh_find "rbh:mongo:$testdb" -not -xattr user.err | sort |
        difflines "/" "/file"
    rbh_find "rbh:mongo:$testdb" -not -xattr err | sort |
        difflines "/" "/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_xattr_exists_no_arg test_xattr_exists
                  test_xattr_not_exists test_not_xattr_exists)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

for test in "${tests[@]}"; do
    (
    trap -- "teardown" EXIT
    setup

    ("$test") && echo "$test: ✔" || echo "$test: ✖"
    )
done
