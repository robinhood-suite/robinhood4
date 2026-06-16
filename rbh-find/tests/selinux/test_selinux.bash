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

test_selinux_simple_filters()
{
    touch file1 file2

    set_selinux_range "s0:c0,c3-s3:c0.c3,c1000" file1
    set_selinux_range "s0:c0,c3-s4:c0.c3" file2

    rbh_sync_selinux "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -name 'file*' \
        -selinux-context "user_u:object_r:user_tmp_t:s0:c0,c3-s4:c0.c3" |
        sort | difflines "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' -selinux-user user_u |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' -selinux-role object_r |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' -selinux-type user_tmp_t |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' \
        -selinux-range "s0:c0,c3-s3:c0.c3,c1000" |
        sort | difflines "/file1"

    rbh_find "rbh:$db:$testdb" -name 'file*' \
        -selinux-range "s0:c0,c3-s4:c0.c3" |
        sort | difflines "/file2"
}

test_selinux_dominates_filter()
{
    touch file1 file2

    set_selinux_range "s0:c0,c3-s3:c0.c3,c1000" file1
    set_selinux_range "s0:c0,c3-s4:c0.c3" file2

    rbh_sync_selinux "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -name 'file*' -selinux-range-dominates "s4" |
        difflines "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' -selinux-range-dominates "s0:c1" |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' \
        -selinux-range-dominates "s3:c0.c3" |
        sort | difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -name 'file*' \
        -selinux-range-dominates "s3:c1000" | difflines "/file1"

    rbh_find "rbh:$db:$testdb" -name 'file*' \
        -selinux-range-dominates "s4:c1000" | difflines
}





################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_selinux_simple_filters
    test_selinux_dominates_filter
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
