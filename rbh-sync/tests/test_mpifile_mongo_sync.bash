#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync_simple_from_dwalk()
{
    touch fileA
    mkdir dir
    touch dir/file1

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:mongo:$testdb"

    find_attribute '"ns.xattrs.path": "'$PWD'"'
    find_attribute '"ns.xattrs.path": "'$PWD'/fileA"'
    find_attribute '"ns.xattrs.path": "'$PWD'/dir"'
    find_attribute '"ns.xattrs.path": "'$PWD'/dir/file1"'
}

test_sync_simple_from_robinhood()
{
    touch fileA
    mkdir dir
    touch dir/file1

    rbh_sync "rbh:posix:." "rbh:mpi-file:$testdb.mfu"
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:mongo:$testdb"

    find_attribute '"ns.xattrs.path": "/"'
    find_attribute '"ns.xattrs.path": "/fileA"'
    find_attribute '"ns.xattrs.path": "/dir"'
    find_attribute '"ns.xattrs.path": "/dir/file1"'
}

test_sync_size()
{
    truncate -s 1025 "fileA"
    local size=$(stat -c %s "fileA")

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:mongo:$testdb"

    find_attribute '"ns.xattrs.path":"'$PWD'/fileA"' '"statx.size":'$size
}

test_sync_time()
{
    touch -d "2 days ago" "fileA"
    local atime=$(stat -c %X "fileA")
    local mtime=$(stat -c %Y "fileA")
    local ctime=$(stat -c %Z "fileA")

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:mongo:$testdb"

    find_attribute '"ns.xattrs.path":"'$PWD'/fileA"' '"statx.atime.sec":'$atime
    find_attribute '"ns.xattrs.path":"'$PWD'/fileA"' '"statx.mtime.sec":'$mtime
    find_attribute '"ns.xattrs.path":"'$PWD'/fileA"' '"statx.ctime.sec":'$ctime
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_simple_from_dwalk test_sync_simple_from_robinhood
                  test_sync_size test_sync_time)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
