#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
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

test_type()
{
    touch fileA
    mkdir dir
    touch dir/fileB
    ln -s dir/fileB fileC

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -type f | sort |
        difflines "/dir/fileB" "/fileA"

    rbh_find "rbh:mpi-file:$testdb.mfu" -type d | sort | difflines "/" "/dir"

    rbh_find "rbh:mpi-file:$testdb.mfu" -type l | sort | difflines "/fileC"
}

test_name()
{
    touch fileA
    touch fileB.c
    touch fileC
    mkdir dir

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -name "fileA" | sort | difflines "/fileA"

    rbh_find "rbh:mpi-file:$testdb.mfu" -name "*.c" | sort | difflines "/fileB.c"

    rbh_find "rbh:mpi-file:$testdb.mfu" -name "file*" | sort |
        difflines "/fileA" "/fileB.c" "/fileC"
}

test_path()
{
    touch fileA
    mkdir dir
    touch dir/fileB dir/fileC

    # We test filtering on path from dwalk and rbh-sync because dwalk store the
    # absolute path unlike rbh-sync, but rbh-find's output is supposed to be the
    # same with dwalk's file and rbh-sync's file.

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -path "/fileA" | sort |
        difflines "/fileA"

    rbh_find "rbh:mpi-file:$testdb.mfu" -path "/dir/*" | sort |
        difflines "/dir/fileB" "/dir/fileC"

    rbh-sync "rbh:posix:." "rbh:mpi-file:$testdb.mfu"

    rbh_find "rbh:mpi-file:$testdb.mfu" -path "/fileA" | sort |
        difflines "/fileA"

    rbh_find "rbh:mpi-file:$testdb.mfu" -path "/dir/*" | sort |
        difflines "/dir/fileB" "/dir/fileC"
}

test_size()
{
    touch empty
    truncate --size 1K 1K
    truncate --size 1025 1K+1
    truncate --size 1M 1M

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -size 1k | sort |
        difflines "/" "/1K"

    rbh_find "rbh:mpi-file:$testdb.mfu" -size +1k | sort |
        difflines "/1K+1" "/1M"

    rbh_find "rbh:mpi-file:$testdb.mfu" -size -1M | sort |
        difflines "/empty"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_type test_name test_path test_size)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
