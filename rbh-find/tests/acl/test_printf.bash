#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/../../../rbh-sync/tests/acl/acl_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_acl_print_directives()
{
    local first_entry="print_acl_dir_1"
    local second_entry="print_acl_dir_2"
    local acl_uid=123456
    local acl_gid=3456
    local expected
    local actual

    mkdir "$first_entry"
    chmod 0754 "$first_entry"

    setfacl -m \
        "u:$acl_uid:rw-,g:$acl_gid:r--,m::rwx" \
        "$first_entry"

    setfacl -d -m \
        "u::rwx,u:$acl_uid:r-x,g::r-x,g:$acl_gid:r--,m::r-x,o::---" \
        "$first_entry"

    mkdir "$second_entry"
    chmod 0710 "$second_entry"

    setfacl -m \
        "u:$acl_uid:r--,g:$acl_gid:--x,m::r-x" \
        "$second_entry"

    setfacl -d -m \
        "u::rwx,u:$acl_uid:rw-,g::---,g:$acl_gid:--x,m::rw-,o::r--" \
        "$second_entry"

    rbh_sync_acl "." "rbh:$db:$testdb"

    expected=$(getfacl -c "$first_entry")

    actual=$(rbh_find "rbh:$db:$testdb" -name "$first_entry" -printf '%RAa%RAd')

    if [ "$actual" != "$expected" ]; then
        error "Unexpected ACL print output:
Expected:
$expected

Got:
$actual"
    fi

    expected=$(getfacl -c -E "$second_entry")

    actual=$(rbh_find "rbh:$db:$testdb" -name "$second_entry" -printf '%RAa%RAd')

    if [ "$actual" != "$expected" ]; then
        error "Unexpected ACL print output:
Expected:
$expected

Got:
$actual"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_acl_print_directives
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
