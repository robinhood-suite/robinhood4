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

test_acl_user()
{
    touch match no_match

    set_access_acl "u:12345:r-x" match
    set_access_acl "u:12346:rwx" match
    set_access_acl "u:12346:r-x" no_match

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-user 12345 | sort |
        difflines "/match"

    rbh_find "rbh:$db:$testdb" \
        -acl-user 12346 | sort |
        difflines "/match" "/no_match"

    rbh_find "rbh:$db:$testdb" \
        -acl-user 12349 | sort |
        difflines
}

test_acl_group()
{
    touch match no_match

    set_access_acl "g:23456:--x" match
    set_access_acl "g:23467:r-x" match
    set_access_acl "g:23467:--x" no_match

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-group 23456 | sort |
        difflines "/match"

    rbh_find "rbh:$db:$testdb" \
        -acl-group 23467 | sort |
        difflines "/match" "/no_match"

    rbh_find "rbh:$db:$testdb" \
        -acl-group 23427 | sort |
        difflines
}

test_acl_default_user()
{
    mkdir match no_match

    set_default_acl "u:424242:-wx" match
    set_default_acl "u:434343:rwx" match
    set_default_acl "u:434343:-wx" no_match

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-default-user 424242 | sort |
        difflines "/match"

    rbh_find "rbh:$db:$testdb" \
        -acl-default-user 434343 | sort |
        difflines "/match" "/no_match"

    rbh_find "rbh:$db:$testdb" \
        -acl-default-user 424262 | sort |
        difflines
}

test_acl_default_group()
{
    mkdir match no_match

    set_default_acl "g:42427:rwx" match
    set_default_acl "g:43438:-wx" match
    set_default_acl "g:43438:rwx" no_match

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-default-group 42427 | sort |
        difflines "/match"

    rbh_find "rbh:$db:$testdb" \
        -acl-default-group 43438 | sort |
        difflines "/match" "/no_match"

    rbh_find "rbh:$db:$testdb" \
        -acl-default-group 42428 | sort |
        difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_acl_user
    test_acl_group
    test_acl_default_user
    test_acl_default_group
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
