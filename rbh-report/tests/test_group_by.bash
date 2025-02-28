#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

main_user_id="$(id -u)"

test_group_by_type()
{
    mkdir first_dir
    mkdir second_dir
    truncate --size 1M first_dir/first_file
    truncate --size 1G second_dir/second_file

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"
    local first_dir_size="$(stat -c %s first_dir)"
    local second_dir_size="$(stat -c %s second_dir)"
    local first_file_size="$(stat -c %s first_dir/first_file)"
    local second_file_size="$(stat -c %s second_dir/second_file)"

    local sum_dir_size="$((root_size + first_dir_size + second_dir_size))"
    local sum_file_size="$((first_file_size + second_file_size))"

    rbh_report --csv "rbh:mongo:$testdb" --group-by "statx.type" \
                                   --output "sum(statx.size)" |
        difflines "directory: $sum_dir_size" "file: $sum_file_size"
}

test_group_by_user()
{
    mkdir first_dir
    mkdir second_dir
    truncate --size 1M first_dir/first_file
    truncate --size 1G second_dir/second_file

    local test_user="$(get_test_user $FUNCNAME)"
    add_test_user $test_user
    local fake_user_id="$(id -u $test_user)"
    chown $test_user: first_dir first_dir/first_file
    delete_test_user $test_user

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"
    local first_dir_size="$(stat -c %s first_dir)"
    local second_dir_size="$(stat -c %s second_dir)"
    local first_file_size="$(stat -c %s first_dir/first_file)"
    local second_file_size="$(stat -c %s second_dir/second_file)"

    local main_user_size="$((root_size + second_dir_size + second_file_size))"
    local fake_user_size="$((first_dir_size + first_file_size))"

    rbh_report --csv "rbh:mongo:$testdb" --group-by "statx.uid" \
                                   --output "sum(statx.size)" |
        difflines "$main_user_id: $main_user_size" \
                  "$fake_user_id: $fake_user_size"
}

test_multi_group_by()
{
    mkdir first_dir
    mkdir second_dir
    truncate --size 1M first_dir/first_file
    truncate --size 1G second_dir/second_file

    local test_user="$(get_test_user $FUNCNAME)"
    add_test_user $test_user
    local fake_user_id="$(id -u $test_user)"
    chown $test_user: first_dir first_dir/first_file
    delete_test_user $test_user

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"
    local first_dir_size="$(stat -c %s first_dir)"
    local second_dir_size="$(stat -c %s second_dir)"
    local first_file_size="$(stat -c %s first_dir/first_file)"
    local second_file_size="$(stat -c %s second_dir/second_file)"

    local main_user_dir_size="$((root_size + second_dir_size))"
    local main_user_file_size="$((second_file_size))"
    local fake_user_dir_size="$((first_dir_size))"
    local fake_user_file_size="$((first_file_size))"

    rbh_report --csv "rbh:mongo:$testdb" --group-by "statx.uid,statx.type" \
                                   --output "sum(statx.size)" |
        difflines "$main_user_id,directory: $main_user_dir_size" \
                  "$main_user_id,file: $main_user_file_size" \
                  "$fake_user_id,directory: $fake_user_dir_size" \
                  "$fake_user_id,file: $fake_user_file_size"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_group_by_type test_group_by_user test_multi_group_by)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
