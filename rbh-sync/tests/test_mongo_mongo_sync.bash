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

function version_code()
{
    eval set -- $(echo $1 | tr "[:punct:]" " ")

    echo -n "$(( (${1:-0} << 16) | (${2:-0} << 8) | ${3:-0} ))"
}

function mongo_version()
{
    local mongo_path="$(which mongo)"
    local version="$($mongo_path --version | head -n 1 | cut -d' ' -f4)"
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

cursordiff_mongo_vers_inf_5_0()
{
    local collection_to_check"=$1"
    local db1="$2"
    local db2="$3"
    local should_fail="$4"
    local script

    printf -v expected 'new Mongo().getDB("%s").%s.find().sort({_id: 1})' \
        "$db1" "$collection_to_check"

    printf -v actual 'new Mongo().getDB("%s").entries.find().sort({_id: 1})' \
        "$db2"

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
    ' "$actual" "$expected"

    if [ "$should_fail" == "true" ]; then
        # "compareCollection" will return 'true' or 'false' is the collections
        # are equal or not, so we use this result as a command
        $(mongosh --nodb --eval "$script") &&
            error "sync should have resulted in different db state\n"
    else
        $(mongosh --nodb --eval "$script") ||
            error "sync resulted in different db state\n"
    fi
}

cursordiff_mongo_vers_sup_eq_5_0()
{
    local collection_to_check"=$1"
    local db1="$2"
    local db2="$3"
    local should_fail="$4"
    local script

    printf -v script '
        function compareCollection(col1, col2){
            if(col1.countDocuments() !== col2.countDocuments()){
                return false;
            }

            var same = true;

            var compared = col1.find().forEach(function(doc1){
                var doc2 = col2.findOne({_id: doc1._id});

                same = same && JSON.stringify(doc1)==JSON.stringify(doc2);
            });

            return same;
        }

        const db1 = db.getSiblingDB("%s");
        const db2 = db.getSiblingDB("%s");

        compareCollection(db1.getCollection("%s"), db2.getCollection("entries"));
    ' "$db1" "$db2" "$collection_to_check"

    if [ "$should_fail" == "true" ]; then
        # "compareCollection" will return 'true' or 'false' is the collections
        # are equal or not, so we use this result as a command
        $(mongosh "$db1" --eval "$script") &&
            error "sync should have resulted in different db state\n"
    else
        $(mongosh "$db1" --eval "$script") ||
            error "sync resulted in different db state\n"
    fi
}

verify_databases_after_sync()
{
    local collection_to_check="$1"
    local should_fail="$2"

    if (( $(mongo_version) < $(version_code 5.0.0) )); then
        cursordiff_mongo_vers_inf_5_0 "$collection_to_check" "$testdb1" \
            "$testdb2" "$should_fail"
    else
        cursordiff_mongo_vers_sup_eq_5_0 "$collection_to_check" "$testdb1" \
            "$testdb2" "$should_fail"
    fi
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

    verify_databases_after_sync "entries" "false"
}

test_sync_branch()
{
    touch fileA
    mkdir dir
    touch dir/fileB

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb1"
    rbh_sync "rbh:mongo:$testdb1#dir" "rbh:mongo:$testdb2"

    mongo --quiet "$testdb1" --eval 'db.entries.aggregate([
        {$match: {"ns.xattrs.path": {$regex: "^/dir"}}},
        {$out: "new_collection"}
    ])'

    verify_databases_after_sync "entries" true
    verify_databases_after_sync "new_collection" false
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
