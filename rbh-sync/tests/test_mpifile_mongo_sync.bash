#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/utils.bash

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync_simple_from_dwalk()
{
    touch fileA
    mkdir dir
    touch dir/fileB

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path": "/"'
    find_attribute '"ns.xattrs.path": "/fileA"'
    find_attribute '"ns.xattrs.path": "/dir"'
    find_attribute '"ns.xattrs.path": "/dir/fileB"'

    entries=("/dir" "/fileA" "/dir/fileB")
    check_id_parent_id ${entries[@]}
}

test_sync_simple_from_robinhood()
{
    touch fileA
    mkdir dir
    touch dir/fileB

    rbh_sync "rbh:posix-mpi:." "rbh:mpi-file:$testdb.mfu"
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path": "/"'
    find_attribute '"ns.xattrs.path": "/fileA"'
    find_attribute '"ns.xattrs.path": "/dir"'
    find_attribute '"ns.xattrs.path": "/dir/fileB"'

    entries=("/dir" "/fileA" "/dir/fileB")
    check_id_parent_id ${entries[@]}
}

test_sync_size()
{
    truncate -s 1025 "fileA"
    local size=$(stat -c %s "fileA")

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.size":'$size
}

test_sync_time()
{
    touch -d "2 days ago" "fileA"
    local atime=$(stat -c %X "fileA")
    local mtime=$(stat -c %Y "fileA")
    local ctime=$(stat -c %Z "fileA")

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.atime.sec":'$atime
    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.mtime.sec":'$mtime
    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.ctime.sec":'$ctime
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
