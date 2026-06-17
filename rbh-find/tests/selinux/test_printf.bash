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
    local root_context="user_u:object_r:user_tmp_t:s0"
    local file_context="user_u:object_r:user_tmp_t:s0:c0,c3-s3:c0.c3,c1000"
    local root_ls_out
    local file_ls_out

    touch file1

    set_selinux_range "s0" "."
    set_selinux_range "s0:c0,c3-s3:c0.c3,c1000" file1

    rbh_sync_selinux "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%RZc\n" | sort |
        difflines "$root_context" "$file_context"

    rbh_find "rbh:$db:$testdb" -printf "%RZu\n" | sort |
        difflines "user_u" "user_u"

    rbh_find "rbh:$db:$testdb" -printf "%RZr\n" | sort |
        difflines "object_r" "object_r"

    rbh_find "rbh:$db:$testdb" -printf "%RZt\n" | sort |
        difflines "user_tmp_t" "user_tmp_t"

    rbh_find "rbh:$db:$testdb" -printf "%RZR\n" | sort |
        difflines "s0" "s0:c0,c3-s3:c0.c3,c1000"

    rbh_find "rbh:$db:$testdb" -printf "%RZu:%RZr:%RZt:%RZR\n" | sort |
        difflines "$root_context" "$file_context"

    root_ls_out=$(ls -Zd . | sed 's# \.$# /#')
    file_ls_out=$(ls -Z file1 | sed 's# file1$# /file1#')

    rbh_find "rbh:$db:$testdb" -printf "%RZc %p\n" | sort |
        difflines "$root_ls_out" "$file_ls_out"
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
