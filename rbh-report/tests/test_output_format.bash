#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

main_user_id="$(id -u)"

complete_setup()
{
    mkdir first_user_dir
    cd first_user_dir
    touch first_file
    ln first_file first_hlink
    ln -s first_file first_slink
    mkfifo first_fifo
    mknod first_block b 0 0
    mknod first_char c 0 0
    cd ..

    mkdir second_user_dir
    cd second_user_dir
    touch second_file
    ln second_file second_hlink
    ln -s second_file second_slink
    mkfifo second_fifo
    mknod second_block b 0 0
    mknod second_char c 0 0
    cd ..

    useradd -MN fake_user || true
    fake_user_id="$(id -u fake_user)"
    chown -R fake_user: second_user_dir
    userdel fake_user || true
}

test_format_multi_group_and_rsort()
{
    complete_setup

    rbh_sync "rbh:posix:first_user_dir" "rbh:mongo:$testdb"
    rbh_sync "rbh:posix:second_user_dir" "rbh:mongo:$testdb"

    local first_root_size="$(stat -c %s first_user_dir)"
    local second_root_size="$(stat -c %s second_user_dir)"
    local first_symlink_size="$(stat -c %s first_user_dir/first_slink)"
    local second_symlink_size="$(stat -c %s second_user_dir/second_slink)"

    rbh_report "rbh:mongo:$testdb" --group-by "statx.uid,statx.type" \
                                   --output "sum(statx.size)" |
        difflines "$main_user_id,fifo: 0" \
                  "$main_user_id,char: 0" \
                  "$main_user_id,directory: $first_root_size" \
                  "$main_user_id,block: 0" \
                  "$main_user_id,file: 0" \
                  "$main_user_id,link: $first_symlink_size" \
                  "$fake_user_id,fifo: 0" \
                  "$fake_user_id,char: 0" \
                  "$fake_user_id,directory: $second_root_size" \
                  "$fake_user_id,block: 0" \
                  "$fake_user_id,file: 0" \
                  "$fake_user_id,link: $second_symlink_size"

    rbh_report "rbh:mongo:$testdb" --group-by "statx.uid,statx.type" \
                                   --output "sum(statx.size)" --rsort |
        difflines "$fake_user_id,link: $second_symlink_size" \
                  "$fake_user_id,file: 0" \
                  "$fake_user_id,block: 0" \
                  "$fake_user_id,directory: $second_root_size" \
                  "$fake_user_id,char: 0" \
                  "$fake_user_id,fifo: 0" \
                  "$main_user_id,link: $first_symlink_size" \
                  "$main_user_id,file: 0" \
                  "$main_user_id,block: 0" \
                  "$main_user_id,directory: $first_root_size" \
                  "$main_user_id,char: 0" \
                  "$main_user_id,fifo: 0"
}

check_output_has_line()
{
    local output="$1"
    shift 1

    for string in "$@"; do
        output="$(echo "$output" | grep "$string")"
        if [ -z "$output" ]; then
            error "'$output' doesn't contain '$string'"
        fi
    done
}

test_format_pretty_print()
{
    complete_setup

    rbh_sync "rbh:posix:first_user_dir" "rbh:mongo:$testdb"
    rbh_sync "rbh:posix:second_user_dir" "rbh:mongo:$testdb"

    local first_root_size="$(stat -c %s first_user_dir)"
    local second_root_size="$(stat -c %s second_user_dir)"
    local first_symlink_size="$(stat -c %s first_user_dir/first_slink)"
    local second_symlink_size="$(stat -c %s second_user_dir/second_slink)"

    local output="$(rbh_report "rbh:mongo:$testdb" --pretty-print \
                        --group-by "statx.uid,statx.type" \
                        --output "sum(statx.size)")"

    check_output_has_line "$output" "statx.uid" "statx.type" "sum(statx.size)"
    check_output_has_line "$output" "$main_user_id" "fifo" "0"
    check_output_has_line "$output" "$main_user_id" "char" "0"
    check_output_has_line "$output" "$main_user_id" "directory" \
                                    "$first_root_size"
    check_output_has_line "$output" "$main_user_id" "block" "0"
    check_output_has_line "$output" "$main_user_id" "file" "0"
    check_output_has_line "$output" "$main_user_id" "link" \
                                    "$first_symlink_size"
    check_output_has_line "$output" "$fake_user_id" "fifo" "0"
    check_output_has_line "$output" "$fake_user_id" "char" "0"
    check_output_has_line "$output" "$fake_user_id" "directory" \
                                    "$second_root_size"
    check_output_has_line "$output" "$fake_user_id" "block" "0"
    check_output_has_line "$output" "$fake_user_id" "file" "0"
    check_output_has_line "$output" "$fake_user_id" "link" \
                                    "$second_symlink_size"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_format_multi_group_and_rsort test_format_pretty_print)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
