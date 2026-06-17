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

test_selinux_print_directives()
{
    touch file1

    set_selinux_range "s0:c0,c3-s3:c0.c3,c1000" file1

    rbh_sync_selinux "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -name file1 -printf "%RZc\n" |
        difflines "user_u:object_r:user_tmp_t:s0:c0,c3-s3:c0.c3,c1000"

    rbh_find "rbh:$db:$testdb" -name file1 -printf "%RZu\n" |
        difflines "user_u"

    rbh_find "rbh:$db:$testdb" -name file1 -printf "%RZr\n" |
        difflines "object_r"

    rbh_find "rbh:$db:$testdb" -name file1 -printf "%RZt\n" |
        difflines "user_tmp_t"

    rbh_find "rbh:$db:$testdb" -name file1 -printf "%RZR\n" |
        difflines "s0:c0,c3-s3:c0.c3,c1000"
}


################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_selinux_print_directives
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
