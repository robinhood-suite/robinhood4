#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"

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

    # We test filtering on path from dwalk and rbh_sync because dwalk store the
    # absolute path unlike rbh_sync, but rbh-find's output is supposed to be the
    # same with dwalk's file and rbh_sync's file.

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -path "/fileA" | sort |
        difflines "/fileA"

    rbh_find "rbh:mpi-file:$testdb.mfu" -path "/dir/*" | sort |
        difflines "/dir/fileB" "/dir/fileC"

    rbh_sync "rbh:posix:." "rbh:mpi-file:$testdb.mfu"

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

test_a_m_time()
{
    local unit=$1
    local predicate=$2

    mkdir dir
    touch dir/fileA
    touch -d "5 $unit ago" dir/fileB
    touch -d "10 $unit ago" dir/fileC

    dwalk -q -o "$testdb.mfu" dir

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" 0 | sort |
        difflines "/" "/fileA"

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" 1 | sort | difflines

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" +2 | sort |
        difflines "/fileB" "/fileC"

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" 6 | sort | difflines

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" -6 | sort |
        difflines "/" "/fileA" "/fileB"

    rm -rf dir
}

test_c_time()
{
    local predicate=$1

    mkdir dir
    touch dir/file

    dwalk -q -o "$testdb.mfu" dir

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" 0 | sort |
        difflines "/" "/file"

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" 1 | sort | difflines

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" +0 | sort |
        difflines "/" "/file"

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" +2 | sort | difflines

    rbh_find "rbh:mpi-file:$testdb.mfu" "$predicate" -3 | sort |
        difflines "/" "/file"

    rm -rf dir
}

test_a_m_time_min()
{
    test_a_m_time "minutes" "-amin"
    test_a_m_time "minutes" "-mmin"
    test_a_m_time "days" "-atime"
    test_a_m_time "days" "-mtime"
}

test_c_time_min()
{
    test_c_time "-cmin"
    test_c_time "-ctime"
}

test_and()
{
    touch empty
    truncate --size 1025 1K+1

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -type f -size +1k | sort |
        difflines "/1K+1"

    rbh_find "rbh:mpi-file:$testdb.mfu" -type d -size -1k | sort | difflines
}

test_not()
{
    touch empty
    truncate --size 1K 1K
    truncate --size 1M 1M

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -not -type d | sort |
        difflines "/1K" "/1M" "/empty"

    rbh_find "rbh:mpi-file:$testdb.mfu" -name "1*" -not -size 1k | sort |
        difflines "/1M"
}

test_or()
{
    touch empty
    truncate --size 1K 1K
    truncate --size 1M 1M

    dwalk -q -o "$testdb.mfu" .

    rbh_find "rbh:mpi-file:$testdb.mfu" -size -1k -o -size +1k | sort |
        difflines "/1M" "/empty"

    rbh_find "rbh:mpi-file:$testdb.mfu" "(" -type f -size +1k ")" -o \
        "(" -type d -size 1k ")" | sort | difflines "/" "/1M"

    rbh_find "rbh:mpi-file:$testdb.mfu" -not "(" -size 1k -o -size +1k ")" |
        sort | difflines "/empty"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_type test_name test_path test_size
                  test_a_m_time_min test_c_time_min test_and
                  test_not test_or)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
