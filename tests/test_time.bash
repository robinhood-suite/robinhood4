#!/usr/bin/env bash

# This file is part of rbh-find.
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
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

test_generic_a_m_time()
{
    touch fileA
    touch -d "5 $1 ago" fileB
    touch -d "10 $1 ago" fileC

    sleep 1

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" "$2" 0 | sort |
        difflines "/" "/fileA"
    rbh_find "rbh:mongo:$testdb" "$2" 1 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" "$2" +2 | sort |
        difflines "/fileB" "/fileC"
    rbh_find "rbh:mongo:$testdb" "$2" 6 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" "$2" -6 | sort |
        difflines "/" "/fileA" "/fileB"
}

test_a_m_time()
{
    test_generic_a_m_time "minutes" "-amin"
    test_generic_a_m_time "minutes" "-mmin"
    test_generic_a_m_time "days" "-atime"
    test_generic_a_m_time "days" "-mtime"
}

test_generic_c_time()
{
    # Unfortunately there is no way to change the "change" time, so there's
    # not much to test
    touch filetest

    sleep 1

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" "$1" 0 | sort |
        difflines "/" "/filetest"
    rbh_find "rbh:mongo:$testdb" "$1" 1 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" "$1" +0 | sort |
        difflines "/" "/filetest"
    rbh_find "rbh:mongo:$testdb" "$1" +2 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" "$1" -3 | sort |
        difflines  "/" "/filetest"

}

test_c_time()
{
    test_generic_c_time "-cmin"
    test_generic_c_time "-ctime"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_a_m_time test_c_time)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
