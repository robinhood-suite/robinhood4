#!/usr/bin/env bash

# This file is part of RobinHood4.
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

    rbh_log "rbh:$db:$testdb" --gc blob &&
        error "log with invalid gc count should have failed"

    rbh_log "rbh:$db:$testdb" --gc 42invalid &&
        error "log with invalid gc count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-gc "rbh-gc rbh:$db:$testdb -size +1M -s 42"

    echo "$output" | grep " deleted " > /dev/null ||
        error "deleted_entries should have been retrieved, got '$output'"

    echo "$output" | grep "non-deleted" > /dev/null ||
        error "non_deleted_entries should have been retrieved, got '$output'"

    echo "$output" | grep "Sync" > /dev/null ||
        error "sync_time should have been retrieved, got '$output'"

    echo "$output" | grep "seen" > /dev/null ||
        error "total_entries should have been retrieved, got '$output'"

}

test_N_logs()
{
    local requested=$1
    local expected=$2
    local count=10

    rbh_sync rbh:posix:. rbh:$db:$testdb
    for i in $(seq 1 $expected); do
        rbh_gc "rbh:$db:$testdb" -size +1M -s 42
    done

    local output=$(rbh_log "rbh:$db:$testdb" --gc $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != $count * $expected)); then
        error "There should be $count * $expected lines about gc ($(($count - 2)) for content, 2 for header/footer, time $expected logs), got '$output'"
    fi

    for i in $(seq 1 $expected); do
        local one_log="$(echo "$output" | head -n $count)"
        check_log_result "$one_log"
        output="$(echo "$output" | sed 1,${count}d)"
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
    check_common_timestamps "--gc" "rbh_gc rbh:$db:$testdb"
}

test_command_line()
{
    local conf="conf"
    local file="file"
    local dir="dir"
    local command="rbh-gc"

    mkdir $dir
    touch $file

    rbh_sync rbh:posix:. rbh:$db:$testdb

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf

    touch blob.sh
    chmod +x blob.sh

    rbh_gc rbh:$db:$testdb > /dev/null
    rbh_gc --config $conf rbh:$db:$testdb --dry-run > /dev/null
    rbh_gc --config $conf rbh:$db:$testdb -s 42 --verbose > /dev/null
    rbh_gc --config $conf rbh:$db:$testdb -type d -size +3 -s 53 > /dev/null
    rbh_gc --dry-run rbh:$db:$testdb --check "$PWD/blob.sh"> /dev/null

    rbh_log rbh:$db:$testdb --gc 6 | grep "Command" | cut -d':' -f2- |
        sed 's/^[ \t]*//' | sed -n "s/.*$command/$command/p" |
        difflines "rbh-gc --dry-run rbh:$db:$testdb --check $PWD/blob.sh" \
                  "rbh-gc --config $conf rbh:$db:$testdb -type d -size +3 -s 53" \
                  "rbh-gc --config $conf rbh:$db:$testdb -s 42 --verbose" \
                  "rbh-gc --config $conf rbh:$db:$testdb --dry-run" \
                  "rbh-gc rbh:$db:$testdb"
}

test_entries_seen_deleted()
{
    local entry1="file1"
    local entry2="file2"
    local entry3="file3"
    local entry4="file4"
    local entry5="file5"

    touch $entry1
    echo "blob" > $entry2
    mkdir $entry3
    ln $entry1 $entry4
    echo "thelazyfoxjumpsoverthebrowndog" > $entry5

    rbh_sync rbh:posix:. rbh:$db:$testdb

    rm $entry1 $entry2 $entry4 $entry5
    rmdir $entry3

    # There are 6 entries in total: the five above + root

    # First gc should see all entries and only remove $entry1
    rbh_gc rbh:$db:$testdb --check "$test_dir/letter_check.sh"
    # Second gc should only see the 2 non-empty files and remove both
    rbh_gc rbh:$db:$testdb -size +0 -type f
    # Third gc should see the 3 remaining entries and remove none
    rbh_gc rbh:$db:$testdb --dry-run
    # Last gc should see the 3 remaining entries and remove 2
    rbh_gc rbh:$db:$testdb

    rbh_log rbh:$db:$testdb --gc 4 | grep " deleted entries" | cut -d':' -f2- |
        sed 's/^[ \t]*//' |
        difflines "2" "0" "2" "1"

    rbh_log rbh:$db:$testdb --gc 4 | grep "non-deleted entries" |
        cut -d':' -f2- | sed 's/^[ \t]*//' |
        difflines "1" "3" "0" "5"

    rbh_log rbh:$db:$testdb --gc 4 | grep "entries seen" | cut -d':' -f2- |
        sed 's/^[ \t]*//' |
        difflines "3" "3" "2" "6"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_last_1 test_last_N test_more_than_N
                  test_timestamps test_command_line test_entries_seen_deleted)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
