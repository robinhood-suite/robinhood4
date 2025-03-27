#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_equal_1K()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb" -size 1k | sort |
        difflines "/" "/1K"
}

test_plus_1K_minus_1M()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    truncate --size 1M "1M"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    # Filtering on size +1k and size -1M is supposed to match nothing, as
    # +1k ensures we only get files of length 2k and more, while -1M
    # only matches files of length 0.
    rbh_lfind "rbh:$db:$testdb" -size +1k -size -1M | sort | difflines
}

test_branch_equal_1K()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1K "dir/1K"
    truncate --size 1025 "dir/1K+1"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb#dir" -size 1k | sort |
        difflines "/dir" "/dir/1K"
}

test_branch_plus_1K_minus_1M()
{
    touch "file"
    mkdir "dir"
    touch "dir/empty"
    truncate --size 1K "dir/1K"
    truncate --size 1025 "dir/1K+1"
    truncate --size 1M "dir/1M"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb#dir" -size +1k -size -1M | sort | difflines
}

test_octal()
{
    local perms=(001 002 003 004 005 006 007
                 010 020 030 040 050 060 070
                 100 200 300 400 500 600 700
                 111 222 333 444 555 666 777)

    for perm in "${perms[@]}"; do
        touch "file.$perm"
        chmod "$perm" "file.$perm"
    done

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    for ((i = 0; i < ${#perms[@]}; i++)); do
        rbh_lfind "rbh:$db:$testdb" -perm "${perms[i]}" | sort |
            difflines "/file.${perms[i]}" || error "incorrect match"
    done
}

test_symbolic()
{
    local perms=(1000 2000 4000 333 444 777 644 111 110 100 004 001)
    local symbolic=(a+t g+s u+s a=wx a=r a=rwx o=r,ug+o,u+w
                    a=x a=x,o-x u=x o=r o=x)

    for perm in "${perms[@]}"; do
        touch "file.$perm"
        chmod "$perm" "file.$perm"
    done

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    for (( i = 0; i < ${#symbolic[@]}; i++ )); do
        rbh_lfind "rbh:$db:$testdb" -perm "${symbolic[i]}" | sort |
            difflines "/file.${perms[i]}" || error "incorrect match"
    done
}

test_xattr_exists()
{
    touch "file"
    setfattr --name user.key --value val file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb" -xattr user | sort |
        difflines "/file"
    rbh_lfind "rbh:$db:$testdb" -xattr user.key | sort |
        difflines "/file"
}

test_xattr_not_exists()
{
    touch "file"
    setfattr --name user.key --value val file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb" -xattr blob | sort | difflines
    rbh_lfind "rbh:$db:$testdb" -xattr user.err | sort | difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_equal_1K test_plus_1K_minus_1M
                  test_branch_equal_1K test_branch_plus_1K_minus_1M
                  test_octal test_symbolic
                  test_xattr_exists test_xattr_not_exists)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
