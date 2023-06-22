#!/usr/bin/env bash

# This file is part of rbh-sync.
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

__rbh_sync=$(PATH="$PWD:$PATH" which rbh-sync)
rbh_sync()
{
    "$__rbh_sync" "$@"
}

__mongosh=$(which mongosh || which mongo)
mongosh()
{
    "$__mongosh" --quiet "$@"
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

cursordiff()
{
    local script

    printf -v script '
        function every(iterable, func) {
            for (item of iterable) {
                if (!func(item))
                    return false;
            }
            return true;
        }

        function* keys(obj) {
            for (key in obj)
                yield key;
        }

        function isEqual(obj1, obj2) {
            if (typeof(obj1) !== typeof(obj2)) {
                return false;
            }
            if (typeof(obj1) === "object") {
                return every(keys(obj1), key => isEqual(obj1[key], obj2[key]));
            }
            return obj1 === obj2;
        }

        function* zip(cur1, cur2) {
            while (cur1.hasNext() && cur2.hasNext()) {
                yield [cur1.next(), cur2.next()];
            }
        }

        const cur1 = %s;
        const cur2 = %s;

        if (cur1.size() != cur2.size())
            quit(1);

        every(zip(cur1, cur2), ([obj1, obj2]) => isEqual(obj1, obj2));
    ' "$1" "$2"

     $(mongosh --nodb --eval "$script") ||
         error "sync resulted in different db state\n"
}

verify_databases_after_sync()
{

    printf -v expected 'new Mongo().getDB("%s").entries.find(%s).sort(
        {_id: 1}
    )' "$testdb1" "$1"

    printf -v actual 'new Mongo().getDB("%s").entries.find().sort({_id: 1})' \
        "$testdb2"

    cursordiff "$actual" "$expected"
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

    verify_databases_after_sync ''
}

test_sync_branch()
{
    touch fileA
    mkdir dir
    touch dir/fileB

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb1"
    rbh_sync "rbh:mongo:$testdb1#dir" "rbh:mongo:$testdb2"

    verify_databases_after_sync '{ "ns.xattrs.path": { $regex: "^/dir" }}'
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
