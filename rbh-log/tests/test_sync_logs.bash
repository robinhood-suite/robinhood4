#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash

################################################################################
#                                    UTILS                                     #
################################################################################

set_permission()
{
    local path=$1
    local sign=$2

    while [[ "$path" != "/home" ]] && [[ "$path" != "/" ]]; do
        if [[ "$sign" == "+" ]]; then
            chmod o+rx $path
        else
            chmod o-rx $path
        fi
        path="$(dirname $path)"
    done
}

sync_with_other_user()
{
    local skip_option=$1
    local path="$(dirname $__rbh_sync)"
    set_permission $path "+"

    local path_config="$(realpath $RBH_CONFIG_PATH)"
    set_permission $path_config "+"

    local output="$(sudo -E -H -u "$test_user" bash -c "\
                    LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
                    $__rbh_sync --config $path_config $skip_option \
                    rbh:posix:. rbh:$db:$testdb" 2>&1)"

    set_permission $path "-"
    set_permission $path_config "-"
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --last blob &&
        error "log with invalid last count should have failed"

    rbh_log "rbh:$db:$testdb" --last 42invalid &&
        error "log with invalid last count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-sync "rbh-sync rbh:posix:. rbh:$db:$testdb"

    local mountpoint=$(echo "$output" | grep "Mountpoint used" |
                       cut -d':' -f2- | xargs)

    if [ "$mountpoint" != "$(pwd)" ]; then
        error "Invalid mountpoint"
    fi

    local converted_entries=$(echo "$output" | grep "converted" |
                              cut -d':' -f2- | xargs)

    local skipped_entries=$(echo "$output" | grep "skipped" |
                            cut -d':' -f2- | xargs)

    local total_entries=$(echo "$output" | grep "seen" |
                          cut -d':' -f2- | xargs)

    local sum_entries=$((skipped_entries + converted_entries))

    local find_entries=$(find . | wc -l)

    if [ "$sum_entries" != "$find_entries" ]; then
        error "The sum of converted and skipped entries does not match the
               number of entries inside the directory"
    fi

    if [ "$sum_entries" != "$total_entries" ]; then
        error "The sum of converted and skipped entries does not match
               total_entries_seen"
    fi
}

test_N_logs()
{
    local requested=$1
    local expected=$2

    for i in $(seq 1 $expected); do
        rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    done

    local output=$(rbh_log "rbh:$db:$testdb" --sync $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 10 * $expected)); then
        error "There should be 10 * $expected lines about posix sync (8 for content, 2 for header/footer, time $expected logs), got '$output'"
    fi

    for i in $(seq 1 $expected); do
        local one_log="$(echo "$output" | head -n 10)"
        check_log_result "$one_log"
        output="$(echo "$output" | sed 1,10d)"
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

test_entry_count()
{
    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    mongo_only_test

    touch $first_file
    touch $second_file
    mkdir $dir
    touch $dir/$third_file

    chmod o-rw $second_file
    chmod o-rw $dir

    # Here, we run a rbh-sync on the files created above as a fake user. Since
    # that user doesn't have the read or write access to the second file and
    # the directory, it cannot synchronize both, the command should skip the
    # second file and the directory
    sync_with_other_user

    local output=$(rbh_log "rbh:$db:$testdb" --sync 1)

    check_expected_log_value "$output" "skipped" "3"
    check_expected_log_value "$output" "converted" "2"
    check_expected_log_value "$output" "seen" "5"

    rbh_sync rbh:posix:. rbh:$db:$testdb
    local output=$(rbh_log "rbh:$db:$testdb" --sync 1)

    check_expected_log_value "$output" "skipped" "0"
    check_expected_log_value "$output" "converted" "5"
    check_expected_log_value "$output" "seen" "5"

    rm -rf $dir
    do_db clear_entries $testdb
    rbh_sync "rbh:posix:." rbh:$db:$testdb
    rbh_log "rbh:$db:$testdb" --sync 3 | grep "Amount" | sort | cut -d':' -f2 |
        # The sort makes it so that all converted counts are shown first,
        # then total count, then skipped
        sed 's/^[ \t]*//' | difflines "2" "3" "5" \
                                      "3" "5" "5" \
                                      "0" "0" "3"
}

test_mountpoint()
{

    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    touch $first_file
    touch $second_file
    mkdir $dir
    touch $dir/$third_file

    rbh_sync rbh:posix:. rbh:$db:$testdb
    local output=$(rbh_log "rbh:$db:$testdb" --sync 1)
    check_expected_log_value "$output" "Mountpoint" "$(pwd)"

    rbh_sync rbh:posix:$(pwd)/$first_file rbh:$db:$testdb
    local output=$(rbh_log "rbh:$db:$testdb" --sync 1)
    check_expected_log_value "$output" "Mountpoint" "$(pwd)/$first_file"

    rbh_sync rbh:posix:$(pwd)/$dir/$third_file rbh:$db:$testdb
    local output=$(rbh_log "rbh:$db:$testdb" --sync 1)
    check_expected_log_value "$output" "Mountpoint" "$(pwd)/$dir/$third_file"

    rbh_sync "rbh:posix:$(pwd)#$dir" rbh:$db:$testdb
    local output=$(rbh_log "rbh:$db:$testdb" --sync 1)
    check_expected_log_value "$output" "Mountpoint" "$(pwd)"

    rbh_sync "rbh:posix:." rbh:$db:$testdb
    rbh_log "rbh:$db:$testdb" --sync 5 | grep "Mountpoint" | cut -d':' -f2 |
        sed 's/^[ \t]*//' | difflines "$(pwd)" \
                                      "$(pwd)" \
                                      "$(pwd)/$dir/$third_file" \
                                      "$(pwd)/$first_file" \
                                      "$(pwd)"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_last_1 test_last_N test_more_than_N
                  test_entry_count test_mountpoint)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
