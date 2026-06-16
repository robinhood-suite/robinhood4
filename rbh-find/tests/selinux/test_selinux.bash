#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/../../../rbh-sync/tests/selinux/selinux_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

setup_selinux_filter_entries()
{
    touch file1 file2 file3

    chcon -u user_u -r object_r -t user_tmp_t \
        -l "s0:c0,c3-s3:c0.c3,c1000" file1

    chcon -u system_u -r object_r -t tmp_t \
        -l "s0:c0,c3-s4:c0.c3" file2

    chcon -u user_u -r object_r -t user_tmp_t \
        -l "s0" file3

    rbh_sync_selinux "." "rbh:$db:$testdb"
}

test_selinux_context_filter()
{
    setup_selinux_filter_entries

    rbh_find "rbh:$db:$testdb" \
        -selinux-context "user_u:object_r:user_tmp_t:s0:c0,c3-s3:c0.c3,c1000" |
        sort | difflines "/file1"

    rbh_find "rbh:$db:$testdb" \
        -selinux-context "system_u:object_r:tmp_t:s0:c0,c3-s4:c0.c3" |
        sort | difflines "/file2"

    rbh_find "rbh:$db:$testdb" \
        -selinux-context "user_u:object_r:user_tmp_t:s0" |
        sort | difflines "/" "/file3"

    rbh_find "rbh:$db:$testdb" \
        -selinux-context "user_u:object_r:user_tmp_t:s9" |
        sort | difflines
}

test_selinux_user_filter()
{
    setup_selinux_filter_entries

    rbh_find "rbh:$db:$testdb" -selinux-user user_u |
        sort | difflines "/" "/file1" "/file3"

    rbh_find "rbh:$db:$testdb" -selinux-user system_u |
        sort | difflines "/file2"

    rbh_find "rbh:$db:$testdb" -selinux-user unknown_u |
        sort | difflines
}

test_selinux_role_filter()
{
    setup_selinux_filter_entries

    rbh_find "rbh:$db:$testdb" -selinux-role object_r |
        sort | difflines "/" "/file1" "/file2" "/file3"

    rbh_find "rbh:$db:$testdb" -selinux-role unknown_r |
        sort | difflines
}

test_selinux_type_filter()
{
    setup_selinux_filter_entries

    rbh_find "rbh:$db:$testdb" -selinux-type user_tmp_t |
        sort | difflines "/" "/file1" "/file3"

    rbh_find "rbh:$db:$testdb" -selinux-type tmp_t |
        sort | difflines "/file2"

    rbh_find "rbh:$db:$testdb" -selinux-type unknown_t |
        sort | difflines
}

test_selinux_range_filter()
{
    setup_selinux_filter_entries

    rbh_find "rbh:$db:$testdb" \
        -selinux-range "s0:c0,c3-s3:c0.c3,c1000" |
        sort | difflines "/file1"

    rbh_find "rbh:$db:$testdb" \
        -selinux-range "s0:c0,c3-s4:c0.c3" |
        sort | difflines "/file2"

    rbh_find "rbh:$db:$testdb" \
        -selinux-range "s0" |
        sort | difflines "/" "/file3"

    rbh_find "rbh:$db:$testdb" \
        -selinux-range "s9" |
        sort | difflines
}

test_selinux_dominates_filter()
{
    setup_selinux_filter_entries

    rbh_find "rbh:$db:$testdb" -selinux-range-dominates "s4" |
        difflines "/file2"

    rbh_find "rbh:$db:$testdb" -selinux-range-dominates "s0:c1" |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb"  \
        -selinux-range-dominates "s3:c0.c3" |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" \
        -selinux-range-dominates "s3:c1000" | difflines "/file1"

    rbh_find "rbh:$db:$testdb" \
        -selinux-range-dominates "s4:c1000" | difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_selinux_context_filter
    test_selinux_user_filter
    test_selinux_role_filter
    test_selinux_type_filter
    test_selinux_range_filter
    test_selinux_dominates_filter
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
