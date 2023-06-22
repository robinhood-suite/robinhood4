#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
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

test_equal_1K()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -size 1k | sort |
        difflines "/" "/1K"
}

test_plus_1K()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    truncate --size 1M "1M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -size +1k | sort |
        difflines "/1K+1" "/1M"
}

test_plus_1K_minus_1M()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    truncate --size 1M "1M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    # Filtering on size +1k and size -1M is supposed to match nothing, as
    # +1k ensures we only get files of length 2k and more, while -1M
    # only matches files of length 0.
    rbh_find "rbh:mongo:$testdb" -size +1k -size -1M | sort | difflines
}

test_equal_1M()
{
    touch "empty"
    truncate --size 1M "1M"
    truncate --size $((1024 * 1024 + 1)) "1M+1"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -size 1M | sort |
        difflines "/" "/1M"
}

test_minus_1M()
{
    touch "empty"
    truncate --size 1M "1M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -size -1M | sort |
        difflines "/empty"
}

test_plus_3M()
{
    touch "empty"
    truncate --size 3M "3M"
    truncate --size 3M "3M+1"
    echo "a" >> "3M+1"
    truncate --size 4M "4M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -size +3M | sort |
        difflines "/3M+1" "/4M"
}

test_plus_1M_minus_2G()
{
    touch "empty"
    truncate --size 4M "4M"
    truncate --size 1M "1.xM"
    echo "hello world!" >> "1.xM"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -size +1M -size -2G | sort |
        difflines "/1.xM" "/4M"
}

test_branch_equal_1K()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1K "dir/1K"
    truncate --size 1025 "dir/1K+1"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size 1k | sort |
        difflines "/dir" "/dir/1K"
}

test_branch_plus_1K()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1K "dir/1K"
    truncate --size 1025 "dir/1K+1"
    truncate --size 1M "dir/1M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size +1k | sort |
        difflines "/dir/1K+1" "/dir/1M"
}

test_branch_plus_1K_minus_1M()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1K "dir/1K"
    truncate --size 1025 "dir/1K+1"
    truncate --size 1M "dir/1M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size +1k -size -1M | sort | difflines
}

test_branch_equal_1M()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1M "dir/1M"
    truncate --size $((1024 * 1024 + 1)) "dir/1M+1"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size 1M | sort |
        difflines "/dir" "/dir/1M"
}

test_branch_minus_1M()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1M "dir/1M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size -1M | sort |
        difflines "/dir/empty"
}

test_branch_plus_3M()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 3M "dir/3M"
    truncate --size 3M "dir/3M+1"
    echo "a" >> "dir/3M+1"
    truncate --size 4M "dir/4M"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size +3M | sort |
        difflines "/dir/3M+1" "/dir/4M"
}

test_branch_plus_1M_minus_2G()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 4M "dir/4M"
    truncate --size 1M "dir/1.xM"
    echo "hello world!" >> dir/1.xM
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb#dir" -size +1M -size -2G | sort |
        difflines "/dir/1.xM" "/dir/4M"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_equal_1K test_plus_1K test_plus_1K_minus_1M
                  test_equal_1M test_minus_1M test_plus_3M
                  test_plus_1M_minus_2G test_branch_equal_1K
                  test_branch_plus_1K test_branch_plus_1K_minus_1M
                  test_branch_equal_1M test_branch_minus_1M
                  test_branch_plus_3M test_branch_plus_1M_minus_2G)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
