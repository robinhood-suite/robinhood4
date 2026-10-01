#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

find_expected_values()
{
    local output="$1"
    local entry_count="$2"
    local exec_count="$3"

    echo "$output" | grep "rbh-find" > /dev/null ||
        error "Should have found 'rbh-find' mentionned, got '$output'"

    echo "$output" | grep "entries" | grep "$entry_count" > /dev/null ||
        error "Last log shown should have found '$entry_count' entries in total, got '$output'"

    if [ ! -z "$exec_count" ]; then
        echo "$output" | grep "command" | grep "$exec_count" > /dev/null ||
            error "Last log shown should have executed command on" \
                  "'$exec_count' entries, got '$output'"
    fi
}

test_stats()
{
    mkdir -p {1..9}/{1..9}

    rbh_sync rbh:posix:. rbh:$db:$testdb

    local output="$(rbh_find --stats rbh:$db:$testdb 2>&1)"
    # 9*9 + 9 + root
    find_expected_values "$output" 91

    rbh_find --stats --stats-file logs.txt rbh:$db:$testdb > /dev/null
    output="$(cat logs.txt)"
    find_expected_values "$output" 91

    output="$(rbh_find --stats rbh:$db:$testdb -exec ls {}/1 ";" 2>&1)"
    # 9*9 + 9 + root, and only 10 directories have a directory named '1' inside
    # of them, the top ones + root.
    find_expected_values "$output" 91 10

    output="$(rbh_find --stats --stats-file syslog rbh:$db:$testdb \
                -exec ls {}/1 ";" 2>&1 1>/dev/null)"
    # Remove the normal stderr lines associated with the exec failures and the
    # 'RobinHood4[<pid>]' header syslog adds.
    output="$(echo "$output" | sed '/cannot access/d')"
    output="$(echo "${output#*: }")"
    journalctl --sync
    local syslog_output="$(journalctl -t RobinHood4 -o cat)"

    journalctl -t RobinHood4 --rotate
    journalctl -t RobinHood4 --vacuum-time=10s

    find_expected_values "$output" 91 10
    if [ "$output" != "$syslog_output" ]; then
        error "Syslog output and stderr should have been the same," \
              "got syslog '$syslog_output' and stderr '$output'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_stats)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
