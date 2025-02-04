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

complete_setup()
{
    mkdir first_user_dir
    cd first_user_dir
    touch first_file
    ln first_file first_hlink
    ln -s first_file first_slink
    mkfifo first_fifo
    mknod first_block b 0 0 || exit 77
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

    fake_user_id="$(id -u $test_user)"
    chown -R $test_user: second_user_dir
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

    rbh_report --csv "rbh:mongo:$testdb" --group-by "statx.uid,statx.type" \
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

    rbh_report --csv "rbh:mongo:$testdb" --group-by "statx.uid,statx.type" \
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
    local line_number="$2"
    shift 2

    # Get the n-th line from the complete output and remove every sequence of
    # more than 1 '|'
    local line="$(echo "$output" | sed -n "$line_number{p;q}" | tr -s '|')"
    local count=0

    # Then check for each argument is that argument is equal to a part of the
    # line, in order
    for string in "$@"; do
        count=$(( count + 1 ))

        echo "$line" | cut -d"|" -f$count | grep "$string"
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

    local output="$(rbh_report "rbh:mongo:$testdb" \
                        --group-by "statx.uid,statx.type" \
                        --output "sum(statx.size)")"

    check_output_has_line "$output" 1 "statx.uid" "statx.type" "sum(statx.size)"
    check_output_has_line "$output" 3 "$main_user_id" "fifo" "0"
    check_output_has_line "$output" 4 "$main_user_id" "char" "0"
    check_output_has_line "$output" 5 "$main_user_id" "directory" \
                                    "$first_root_size"
    check_output_has_line "$output" 6 "$main_user_id" "block" "0"
    check_output_has_line "$output" 7 "$main_user_id" "file" "0"
    check_output_has_line "$output" 8 "$main_user_id" "link" \
                                    "$first_symlink_size"
    check_output_has_line "$output" 9 "$fake_user_id" "fifo" "0"
    check_output_has_line "$output" 10 "$fake_user_id" "char" "0"
    check_output_has_line "$output" 11 "$fake_user_id" "directory" \
                                    "$second_root_size"
    check_output_has_line "$output" 12 "$fake_user_id" "block" "0"
    check_output_has_line "$output" 13 "$fake_user_id" "file" "0"
    check_output_has_line "$output" 14 "$fake_user_id" "link" \
                                    "$second_symlink_size"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_format_multi_group_and_rsort test_format_pretty_print)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user || true" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
