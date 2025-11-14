#!/usr/bin/env bash

# This file is part of rbh-undelete.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_simple_update_path()
{
    mkdir dir
    touch dir/file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    do_db drop_path $testdb "/dir" "new_dir"

    rbh_update "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/new_dir" "/new_dir/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_simple_update_path)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
