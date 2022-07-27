#!/usr/bin/env bash

# This file is part of rbh-find.
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

test_xattr_exists_no_arg()
{
    touch "file"
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    ! rbh_find "rbh:mongo:$testdb" -xattr
}

test_xattr_exists()
{
    touch "file"
    setfattr --name user.key --value val file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -xattr user | sort |
        difflines "/file"
    rbh_find "rbh:mongo:$testdb" -xattr user.key | sort |
        difflines "/file"
}

test_xattr_not_exists()
{
    touch "file"
    setfattr --name user.key --value val file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -xattr blob | sort | difflines
    rbh_find "rbh:mongo:$testdb" -xattr user.err | sort | difflines
}

test_not_xattr_exists()
{
    touch "file"
    setfattr --name user.key --value val file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -not -xattr user | sort |
        difflines "/"
    rbh_find "rbh:mongo:$testdb" -not -xattr user.key | sort |
        difflines "/"
    rbh_find "rbh:mongo:$testdb" -not -xattr user.err | sort |
        difflines "/" "/file"
    rbh_find "rbh:mongo:$testdb" -not -xattr err | sort |
        difflines "/" "/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_xattr_exists_no_arg test_xattr_exists
                  test_xattr_not_exists test_not_xattr_exists)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
