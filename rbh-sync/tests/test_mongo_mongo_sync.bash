#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

setup_double_db()
{
    # Create test databases' names
    testdb1=$SUITE-1$test
    testdb2=$SUITE-2$test
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

    rbh_sync "rbh:posix:." "rbh:$db:$testdb1"
    rbh_sync "rbh:$db:$testdb1" "rbh:$db:$testdb2"

    local db1entries="$(mongo "$testdb1" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"
    local db2entries="$(mongo "$testdb2" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"

    mongo "$testdb1" --eval "db.dropDatabase()"
    mongo "$testdb2" --eval "db.dropDatabase()"

    if [ "$db1entries" != "$db2entries" ]; then
        error "sync resulted in different db state\n"
    fi
}

test_sync_branch()
{
    touch fileA
    mkdir dir
    touch dir/fileB

    rbh_sync "rbh:posix:." "rbh:$db:$testdb1"
    rbh_sync "rbh:$db:$testdb1#dir" "rbh:$db:$testdb2"

    local db1entries="$(mongo "$testdb1" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"
    local db2entries="$(mongo "$testdb2" --eval \
        "db.entries.find({}, {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"

    if [ "$db1entries" == "$db2entries" ]; then
        mongo "$testdb1" --eval "db.dropDatabase()"
        mongo "$testdb2" --eval "db.dropDatabase()"

        error "sync should have resulted in different db state\n"
    fi

    db1entries="$(mongo "$testdb1" --eval \
        "db.entries.find({\"ns.xattrs.path\": {\$regex: \"^/dir\"}},
                         {\"_id\": 0, \"ns.parent\": 0})
                   .sort({\"ns.name\": 1})")"

    mongo "$testdb1" --eval "db.dropDatabase()"
    mongo "$testdb2" --eval "db.dropDatabase()"

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

sub_setup=setup_double_db
run_tests ${tests[@]}
