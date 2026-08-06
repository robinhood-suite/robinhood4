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
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --report blob &&
        error "log with invalid report count should have failed"

    rbh_log "rbh:$db:$testdb" --report 42invalid &&
        error "log with invalid report count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-report \
        "rbh-report rbh:$db:$testdb --group-by statx.uid --output sum(statx.size)"
}

test_N_logs()
{
    local requested=$1
    local expected=$2
    local count=6

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    for i in $(seq 1 $expected); do
        rbh_report "rbh:$db:$testdb" \
            --group-by "statx.uid" --output "sum(statx.size)" > /dev/null
    done

    local output=$(rbh_log "rbh:$db:$testdb" --report $requested)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != $count * $expected)); then
        error "There should be $count * $expected lines about report ($(($count - 2)) for content, 2 for header/footer, time $expected logs), got '$output'"
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
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    check_common_timestamps "--report" \
        "rbh_report rbh:$db:$testdb
            --group-by \"statx.uid\" --output \"sum(statx.size)\""
}

test_command_line()
{
    local conf="conf"
    local file="file"
    local dir="dir"
    local command="rbh-report"

    mkdir $dir
    touch $file

    rbh_sync rbh:posix:. rbh:$db:$testdb

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf

    rbh_report rbh:$db:$testdb \
            --group-by "statx.uid" --output "sum(statx.size)" > /dev/null
    rbh_report --config $conf rbh:$db:$testdb \
            --group-by "statx.uid" --output "sum(statx.size)" > /dev/null
    rbh_report --config $conf rbh:$db:$testdb -v \
            --group-by "statx.uid" --output "sum(statx.size)" > /dev/null
    rbh_report rbh:$db:$testdb \
            --group-by "statx.uid" --output "sum(statx.size)" --csv > /dev/null
    rbh_report rbh:$db:$testdb \
            --group-by "statx.uid,statx.gid,statx.type" \
            --output "sum(statx.size),min(statx.uid),avg(statx.gid)" \
            --csv --rsort > /dev/null

    rbh_log rbh:$db:$testdb --report 5 | grep "Command" | cut -d':' -f2- |
        sed 's/^[ \t]*//' | sed -n "s/.*$command/$command/p" |
        difflines "rbh-report rbh:$db:$testdb --group-by statx.uid,statx.gid,statx.type --output sum(statx.size),min(statx.uid),avg(statx.gid) --csv --rsort" \
                  "rbh-report rbh:$db:$testdb --group-by statx.uid --output sum(statx.size) --csv" \
                  "rbh-report --config $conf rbh:$db:$testdb -v --group-by statx.uid --output sum(statx.size)" \
                  "rbh-report --config $conf rbh:$db:$testdb --group-by statx.uid --output sum(statx.size)" \
                  "rbh-report rbh:$db:$testdb --group-by statx.uid --output sum(statx.size)"
}

test_order()
{
    local order="$1"

    local command="rbh-report"
    local flag="report"

    if [ "$order" == "descending" ]; then
        local count="3"
    else
        local count="-3"
    fi

    rbh_sync rbh:posix:. rbh:$db:$testdb

    rbh_report rbh:$db:$testdb \
            --group-by "statx.uid" --output "sum(statx.size)" > /dev/null
    rbh_report rbh:$db:$testdb -v --csv \
            --group-by "statx.uid" --output "sum(statx.size)" > /dev/null
    rbh_report rbh:$db:$testdb \
            --group-by "statx.uid,statx.gid,statx.type" \
            --output "sum(statx.size),min(statx.uid),avg(statx.gid)" \
            --csv --rsort > /dev/null

    local output="$(rbh_log rbh:$db:$testdb --$flag $count | grep "Command" |
                    cut -d':' -f2- | sed 's/^[ \t]*//' |
                    sed -n "s/.*$command/$command/p")"

    if [ "$order" == "descending" ]; then
        echo "$output" |
            difflines "rbh-report rbh:$db:$testdb --group-by statx.uid,statx.gid,statx.type --output sum(statx.size),min(statx.uid),avg(statx.gid) --csv --rsort" \
                      "rbh-report rbh:$db:$testdb -v --csv --group-by statx.uid --output sum(statx.size)" \
                      "rbh-report rbh:$db:$testdb --group-by statx.uid --output sum(statx.size)"
    else
        echo "$output" |
            difflines "rbh-report rbh:$db:$testdb --group-by statx.uid --output sum(statx.size)" \
                      "rbh-report rbh:$db:$testdb -v --csv --group-by statx.uid --output sum(statx.size)" \
                      "rbh-report rbh:$db:$testdb --group-by statx.uid,statx.gid,statx.type --output sum(statx.size),min(statx.uid),avg(statx.gid) --csv --rsort"
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

declare -a tests=(test_invalid test_last_1 test_last_N test_more_than_N
                  test_timestamps test_command_line test_ascending
                  test_descending)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
