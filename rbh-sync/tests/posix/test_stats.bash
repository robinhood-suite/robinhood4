#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/posix_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid_stat_options()
{
    rbh_sync rbh:posix:$file rbh:$db:$testdb --stats --log-timer blob &&
        error "Sync with invalid log timer should have failed"

    rbh_sync rbh:posix:$file rbh:$db:$testdb --stats --log-timer -3 &&
        error "Sync with invalid log timer should have failed"

    return 0
}

find_expected_values()
{
    local output="$1"
    local entry_count="$2"
    local file_count="$3"

    echo "$output" | grep "rbh-sync" > /dev/null ||
        error "Should have found 'rbh-sync' mentionned, got '$output'"

    echo "$output" | grep "progress" | grep "$entry_count" > /dev/null ||
        error "Last log shown should have found '$entry_count' entries in total, got '$output'"

    echo "$output" | grep "directories seen" | grep "91" > /dev/null ||
        error "Last log shown should have found 91 directories in total, got '$output'"

    echo "$output" | grep "files seen" | grep "$file_count" > /dev/null ||
        error "Last log shown should have found '$file_count' files in total, got '$output'"

    echo "$output" | grep "links seen" | grep "1" > /dev/null ||
        error "Last log shown should have found 1 symlink in total, got '$output'"

    echo "$output" | grep "current speed" | grep "entries/sec" > /dev/null ||
        error "Logs should show the current speed in entries per second, got '$output'"
}

test_stats()
{
    mkdir -p {1..9}/{1..9}
    touch blob
    touch something
    ln -s something else

    local output="$(rbh_sync rbh:posix:. rbh:$db:$testdb --stats)"
    find_expected_values "$output" 94 2

    rbh_sync rbh:posix:. rbh:$db:$testdb --stats --log-file logs.txt
    output="$(cat logs.txt)"
    # 94 + 1 because of logs.txt
    find_expected_values "$output" 95 3
}

test_mpi_stats()
{
    local cpu_count=$(nproc)

    mkdir -p {1..9}/{1..9}
    touch blob
    touch something
    ln -s something else

    local output="$(rbh_sync rbh:posix-mpi:. rbh:$db:$testdb --stats)"
    # Each MPI process will output at the end of the command, there are
    # $cpu_count processes being run, each output 'block' has 14 lines, and
    # there are 2 lines in-between each block.
    output="$(echo "$output" |
              tail -n $((cpu_count * 14 + (cpu_count - 1) * 2)))"

    echo "$output" | grep "rbh-sync" > /dev/null ||
        error "Should have found 'rbh-sync' mentionned, got '$output'"

    local entry_count="$(echo "$output" | grep "progress" | cut -d':' -f2 |
                         cut -d' ' -f2)"

    entry_count="$(echo "$entry_count" | paste -s -d+ - | bc)"

    # 9 * 9 + 9 directories + root + 2 files + 1 symlink = 94 entries
    if (( entry_count != 94 )); then
        error "MPI commands should have found 94 entries in total, got '$output'"
    fi

    echo "$output" | grep "current speed" | grep "entries/sec" > /dev/null ||
        error "Logs should show the current speed in entries per second, got '$output'"

    local file_count="$(echo "$output" | grep "files seen" | cut -d':' -f2 |
                        cut -d' ' -f2)"
    file_count="$(echo "$file_count" | paste -s -d+ - | bc)"
    if (( file_count != 2 )); then
        error "MPI commands should have found 2 files in total, got '$output'"
    fi

    local dir_count="$(echo "$output" | grep "directories seen" |
                       cut -d':' -f2 | cut -d' ' -f2)"
    dir_count="$(echo "$dir_count" | paste -s -d+ - | bc)"
    if (( dir_count != 91 )); then
        error "MPI commands should have found 91 dirs in total, got '$output'"
    fi

    local symlink_count="$(echo "$output" | grep "links seen" | cut -d':' -f2 |
                           cut -d' ' -f2)"
    symlink_count="$(echo "$symlink_count" | paste -s -d+ - | bc)"
    if (( symlink_count != 1 )); then
        error "MPI commands should have found 1 symlinks in total, got '$output'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid_stat_options)

if [[ $WITH_MPI == true ]]; then
    tests+=(test_mpi_stats)
else
    tests+=(test_stats)
fi

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
