#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -e

################################################################################
#                                  UTILITIES                                   #
################################################################################

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

__rbh_sync=$(PATH="$PWD/rbh-sync:$PATH" which rbh-sync)
rbh_sync()
{
    "$__rbh_sync" "$@"
}

__mongosh=$(which mongosh || which mongo)
mongosh()
{
    "$__mongosh" --quiet "$@"
}

function version_code()
{
    eval set -- $(echo $1 | tr "[:punct:]" " ")

    echo -n "$(( (${1:-0} << 16) | (${2:-0} << 8) | ${3:-0} ))"
}

function mongo_version()
{
    local version="$(mongod --version | head -n 1 | cut -d' ' -f4)"
    version_code "${version:1}"
}

setup()
{
    # Create a test directory and `cd` into it
    testdir=$SUITE-$test
    mkdir "$testdir"
    cd "$testdir"

    # Create test databases' names
    testdb1=$SUITE-1$test
    testdb2=$SUITE-2$test
}

teardown()
{
    mongosh "$testdb1" --eval "db.dropDatabase()" >/dev/null
    mongosh "$testdb2" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
}

error()
{
    printf "$@" >&2
    exit 1
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync_simple()
{
    touch fileA
    touch fileB
    mkdir dir
    touch dir/file1

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb1"
    rbh_sync "rbh:mongo:$testdb1" "rbh:mongo:$testdb2"

    local db1entries="$(mongosh "$testdb1" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"
    local db2entries="$(mongosh "$testdb2" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"

    if [ "$db1entries" != "$db2entries" ]; then
        error "sync resulted in different db state\n"
    fi
}

test_sync_branch()
{
    touch fileA
    mkdir dir
    touch dir/fileB

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb1"
    rbh_sync "rbh:mongo:$testdb1#dir" "rbh:mongo:$testdb2"

    local db1entries="$(mongosh "$testdb1" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"
    local db2entries="$(mongosh "$testdb2" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"

    if [ "$db1entries" == "$db2entries" ]; then
        error "sync should have resulted in different db state\n"
    fi

    db1entries="$(mongosh "$testdb1" --eval \
        "db.entries.find({\"ns.xattrs.path\": {\$regex: \"^/dir\"}},
                         {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"

    if [ "$db1entries" != "$db2entries" ]; then
        error "sync with branching resulted in different db state\n"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_simple test_sync_branch)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

for test in "${tests[@]}"; do
    (
    trap -- "teardown" EXIT
    setup

    ("$test") && echo "$test: ✔" || error "$test: ✖\n"
    )
done
