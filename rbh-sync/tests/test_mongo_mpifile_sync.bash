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

test_sync_simple()
{
    touch fileA
    touch fileB
    mkdir dir
    touch dir/file1

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    rbh_sync "rbh:mongo:$testdb" "rbh:mpi-file:$testdb"

    dfind -i "$testdb" --print -q | sort | \
        difflines "/" "/dir" "/dir/file1" "/fileA" "/fileB"
}

test_sync_time()
{
    touch fileA
    touch -d "2 $1 ago" fileB
    touch -d "4 $1 ago" fileC

    sleep 1

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    rbh_sync "rbh:mongo:$testdb" "rbh:mpi-file:$testdb"

    dfind -i "$testdb" "$2" 0 --print -q | sort | difflines "/" "/fileA"

    dfind -i "$testdb" "$2" +1 --print -q | sort | difflines "/fileB" "/fileC"

    dfind -i "$testdb" "$2" 2 --print -q | sort | difflines "/fileB"

    dfind -i "$testdb" "$2" +4 --print -q | sort | difflines

    dfind -i "$testdb" "$2" -4 --print -q | sort | \
        difflines "/" "/fileA" "/fileB"
}

test_sync_atime()
{
    test_sync_time "days" "--atime"
}

test_sync_amin()
{
    test_sync_time "minutes" "--amin"
}

test_sync_mtime()
{
    test_sync_time "days" "--mtime"
}

test_sync_mmin()
{
    test_sync_time "minutes" "--mmin"
}

test_sync_c_time()
{
    touch file

    sleep 1

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    rbh_sync "rbh:mongo:$testdb" "rbh:mpi-file:$testdb"

    dfind -i "$testdb" "$1" 0 --print -q | sort | difflines "/" "/file"

    dfind -i "$testdb" "$1" 1 --print -q | sort | difflines

    dfind -i "$testdb" "$1" +0 --print -q | sort | difflines

    dfind -i "$testdb" "$1" -2 --print -q | sort | difflines "/" "/file"
}

test_sync_ctime()
{
    test_sync_c_time "--ctime"
}

test_sync_cmin()
{
    test_sync_c_time "--cmin"
}

test_sync_size()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    truncate --size 1M "1M"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    rbh_sync "rbh:mongo:$testdb" "rbh:mpi-file:$testdb"

    dfind -i $testdb --size 1K --print -q | sort | difflines "/1K"

    dfind -i $testdb --size +1K --print -q | sort | \
        difflines "/1K+1" "/1M"

    dfind -i $testdb --size -1K --print -q | sort | difflines "/" "/empty"

    dfind -i $testdb --size +1M --print -q | sort | difflines
}

test_sync_type()
{
    touch fileA
    mkdir dir
    touch dir/file1
    ln -s dir/file1 fileB

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    rbh_sync "rbh:mongo:$testdb" "rbh:mpi-file:$testdb"

    dfind -i $testdb --type d --print -q | sort | difflines "/" "/dir"

    dfind -i $testdb --type f --print -q | sort | \
        difflines "/dir/file1" "/fileA"

    dfind -i $testdb --type l --print -q | sort | difflines "/fileB"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_simple test_sync_atime test_sync_amin
                  test_sync_mtime test_sync_mmin test_sync_ctime
                  test_sync_cmin test_sync_size test_sync_type)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
