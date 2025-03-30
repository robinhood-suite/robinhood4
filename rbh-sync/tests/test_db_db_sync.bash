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

teardown_dbs()
{
    do_db drop "$testdb1"
    do_db drop "$testdb2"
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

    local db1entries="$(do_db dump "$testdb1")"
    local db2entries="$(do_db dump "$testdb2")"

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

    local db1entries="$(do_db dump "$testdb1")"
    local db2entries="$(do_db dump "$testdb2")"

    if [ "$db1entries" == "$db2entries" ]; then
        error "sync should have resulted in different db state\n"
    fi

    db1entries="$(do_db dump "$testdb1" \
        '"ns.xattrs.path": {$regex: "^/dir"}')"

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
sub_teardown=teardown_dbs
run_tests ${tests[@]}
