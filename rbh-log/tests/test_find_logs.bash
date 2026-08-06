#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --find blob &&
        error "log with invalid find count should have failed"

    rbh_log "rbh:$db:$testdb" --find 42invalid &&
        error "log with invalid find count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"
    local with_exec="$2"

    if [ "$with_exec" == "true" ]; then
        check_common_logs "$output" rbh-find \
            "rbh-find rbh:$db:$testdb -exec ls {} ;"
    else
        check_common_logs "$output" rbh-find "rbh-find rbh:$db:$testdb"
    fi

    echo "$output" | grep "post-filtering" > /dev/null ||
        error "entry_count should have been retrieved"

    if [ "$with_exec" == "true" ]; then
        echo "$output" | grep "exec" > /dev/null ||
            error "exec_success_count should have been retrieved"
    fi
}

test_N_logs()
{
    local requested=$1
    local expected=$2

    local with_exec=$(( expected / 2 ))
    local without_exec=$(( expected - with_exec ))
    local expected_line_count=0

    rbh_sync rbh:posix:. rbh:$db:$testdb
    for i in $(seq 1 $with_exec); do
        rbh_find "rbh:$db:$testdb" -exec ls "{}" \; > /dev/null
        expected_line_count=$(( expected_line_count + 6 + 2 ))
    done

    for i in $(seq 1 $without_exec); do
        rbh_find "rbh:$db:$testdb" > /dev/null
        expected_line_count=$(( expected_line_count + 5 + 2 ))
    done

    local output=$(rbh_log "rbh:$db:$testdb" --find $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != $expected_line_count)); then
        error "There should be $expected_line_count lines about find, got '$output'"
    fi

    for i in $(seq 1 $without_exec); do
        local one_log="$(echo "$output" | head -n 7)"
        check_log_result "$one_log" false
        output="$(echo "$output" | sed 1,7d)"
    done

    for i in $(seq 1 $with_exec); do
        local one_log="$(echo "$output" | head -n 8)"
        check_log_result "$one_log" true
        output="$(echo "$output" | sed 1,8d)"
    done
}

test_last_1()
{
    test_N_logs 1 1
}

test_last_N()
{
    test_N_logs 3 3
}

test_more_than_N()
{
    test_N_logs 6 3
}

test_timestamps()
{
    rbh_sync rbh:posix:. rbh:$db:$testdb
    check_common_timestamps "--find" "rbh_find rbh:$db:$testdb"
}

test_command_line()
{
    local conf="conf"
    local file="file"
    local dir="dir"
    local command="rbh-find"

    mkdir $dir
    touch $file

    rbh_sync rbh:posix:. rbh:$db:$testdb

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
 alias:
     blob: \"-size +42 -user root\"
---" > $conf

    rbh_find rbh:$db:$testdb > /dev/null
    rbh_find --config $conf rbh:$db:$testdb > /dev/null
    rbh_find --config $conf --verbose rbh:$db:$testdb -size +3 > /dev/null
    rbh_find --config $conf rbh:$db:$testdb -size +3 -uid 30 -printf "blob\n" > /dev/null
    rbh_find --config $conf rbh:$db:$testdb --alias blob -count > /dev/null

    rbh_log rbh:$db:$testdb --find 6 | grep "Command" | cut -d':' -f2- |
        sed 's/^[ \t]*//' | sed -n "s/.*$command/$command/p" |
        difflines "rbh-find --config $conf rbh:$db:$testdb --alias blob -count" \
                  "rbh-find --config $conf rbh:$db:$testdb -size +3 -uid 30 -printf blob\n" \
                  "rbh-find --config $conf --verbose rbh:$db:$testdb -size +3" \
                  "rbh-find --config $conf rbh:$db:$testdb" \
                  "rbh-find rbh:$db:$testdb"
}

test_entry_count()
{
    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    touch $first_file
    echo "blob" > $second_file
    mkdir $dir
    touch $dir/$third_file

    rbh_sync rbh:posix:. rbh:$db:$testdb

    rbh_find rbh:$db:$testdb > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "post-filtering" "5"

    rbh_find rbh:$db:$testdb -name "*test*" > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "post-filtering" "3"

    rbh_find rbh:$db:$testdb -type d > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "post-filtering" "2"

    rbh_find rbh:$db:$testdb -type f -size +0 > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "post-filtering" "1"

    rbh_log "rbh:$db:$testdb" --find 4 | grep "post-filtering" | cut -d':' -f2 |
        sed 's/^[ \t]*//' | difflines "1" "2" "3" "5"
}

test_exec_success_count()
{
    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    touch $first_file
    echo "blob" > $second_file
    mkdir $dir
    echo "something" > $dir/$third_file

    rbh_sync rbh:posix:. rbh:$db:$testdb

    rbh_find rbh:$db:$testdb -exec cat {} \; > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "exec command" "3"

    rbh_find rbh:$db:$testdb -exec grep -H "test" {} \; > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "exec command" "0"

    rbh_find rbh:$db:$testdb -exec grep -H "blob" {} \; > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "exec command" "1"

    rbh_find rbh:$db:$testdb -type f -exec grep -H "o" {} \; > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "exec command" "2"

    rbh_find rbh:$db:$testdb -type l -exec echo "{}" \; > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "exec command" "0"

    rbh_find rbh:$db:$testdb -exec ls {} \; > /dev/null
    local output=$(rbh_log "rbh:$db:$testdb" --find 1)
    check_expected_log_value "$output" "exec command" "5"

    rbh_log "rbh:$db:$testdb" --find 6 | grep "exec command" | cut -d':' -f2 |
        sed 's/^[ \t]*//' | difflines "5" "0" "2" "1" "0" "3"
}

test_order()
{
    local order="$1"

    local command="rbh-find"
    local flag="find"

    if [ "$order" == "descending" ]; then
        local count="4"
    else
        local count="-4"
    fi

    rbh_sync rbh:posix:. rbh:$db:$testdb

    rbh_find rbh:$db:$testdb > /dev/null
    rbh_find --verbose rbh:$db:$testdb -size +3 > /dev/null
    rbh_find rbh:$db:$testdb -size +3 -uid 30 -printf "blob\n" > /dev/null
    rbh_find rbh:$db:$testdb -count > /dev/null

    local output="$(rbh_log rbh:$db:$testdb --$flag $count | grep "Command" |
                    cut -d':' -f2- | sed 's/^[ \t]*//' |
                    sed -n "s/.*$command/$command/p")"

    if [ "$order" == "descending" ]; then
        echo "$output" |
            difflines "rbh-find rbh:$db:$testdb -count" \
                      "rbh-find rbh:$db:$testdb -size +3 -uid 30 -printf blob\n" \
                      "rbh-find --verbose rbh:$db:$testdb -size +3" \
                      "rbh-find rbh:$db:$testdb"
    else
        echo "$output" |
            difflines "rbh-find rbh:$db:$testdb" \
                      "rbh-find --verbose rbh:$db:$testdb -size +3" \
                      "rbh-find rbh:$db:$testdb -size +3 -uid 30 -printf blob\n" \
                      "rbh-find rbh:$db:$testdb -count"
    fi
}

test_ascending()
{
    test_order ascending
}

test_descending()
{
    test_order descending
}

################################################################################
#                                     MAIN                                     #
################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_last_1 test_last_N test_more_than_N
                  test_timestamps test_command_line test_entry_count
                  test_exec_success_count test_ascending test_descending)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
