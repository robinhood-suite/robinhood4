#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_oneline()
{
    local order=$1

    generate_commands

    local output=$(rbh_log "rbh:$db:$testdb" $order -n 21 --oneline)
    local tmp_output=$(rbh_log "rbh:$db:$testdb" $order -n 30 --oneline)

    if [ "$output" != "$tmp_output" ]; then
        error "Outputted oneline logs should have been the same, got '$output' and '$tmp_output'"
    fi

    while [ ! -z "$output" ]; do
        local log="$(echo "$output" | head -n 1)"

        local expected_output=("Start of the command"
                               "Duration of the command")
        if [[ $log == *"rbh-sync"* ]]; then
            expected_output+=("Amount of entries converted"
                              "Amount of entries seen")
        elif [[ $log == *"rbh-find"* ]]; then
            expected_output+=("Number of entries post-filtering")
        elif [[ $log == *"rbh-fsevents"* ]]; then
            expected_output+=("Amount of changelog read"
                              "Time spent reading/deduplicating events"
                              "Time spent enriching/updating mirror")
        elif [[ $log == *"rbh-report"* ]]; then
            # Nothing to add here
            echo "blob"
        elif [[ $log == *"rbh-gc"* ]]; then
            expected_output+=("Amount of deleted entries"
                              "Amount of entries seen")
        else
            error "Invalid command found: '$log'"
        fi

        if [ "${log:0:1}" != "{" ] || [ "${log: -1}" != "}" ]; then
            error "Missing initial '{' or ending '}' in '$log'"
        fi

        log="${log:2}"
        log="${log::-2}"

        if [[ "$(echo "$log" | cut -d':' -f1)" != *"rbh-"* ]]; then
            error "First value in log should have been a 'rbh' command, got '$log'"
        fi

        log="$(echo "$log" | cut -d':' -f2-)"

        export IFS=","
        for info in $log; do
            local found=""
            local new_expected=()

            for i in "${!expected_output[@]}"; do
                if [[ "$info" == *"${expected_output[$i]}"* ]]; then
                    found=$i
                else
                    new_expected+=("${expected_output[$i]}")
                fi
            done

            if [ -z "$found" ]; then
                error "Unknown info found in log, got '$info'"
            fi

            expected_output=(${new_expected[@]})
        done

        if [[ "${#expected_output[@]}" != "0" ]]; then
            error "Failed to find all expected info in '$log', missing '${expected_output[@]}'"
        fi

        output="$(echo "$output" | tail -n +2)"
    done
}

test_csv()
{
    generate_commands

    local output=$(rbh_log "rbh:$db:$testdb" -n 21 --csv)
    local tmp_output=$(rbh_log "rbh:$db:$testdb" -n 30 --csv)

    if [ "$output" != "$tmp_output" ]; then
        error "Outputted CSV logs should have been the same, got '$output' and '$tmp_output'"
    fi

    while [ ! -z "$output" ]; do
        local log="$(echo "$output" | head -n 1)"

        # 1 for the command + 4 for the common logs info
        local expected_column_count=5
        local regexp


        if [[ $log == *"rbh-sync"* ]]; then
            expected_column_count=$((expected_column_count + 4))
            regexp="rbh-sync,.*,.*,.*,\"$__rbh_sync rbh:posix:. rbh:$db:$testdb\",\"$PWD\",1,0,1"
        elif [[ $log == *"rbh-find"* ]]; then
            expected_column_count=$((expected_column_count + 2))
            regexp="rbh-find,.*,.*,.*,\"$__rbh_find rbh:$db:$testdb -exec ls ;\",[0-9]+,[0-9]+"
        elif [[ $log == *"rbh-fsevents"* ]]; then
            expected_column_count=$((expected_column_count + 9))
            regexp="rbh-fsevents,.*,.*,.*,\"$__rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb\",\"$LUSTRE_MDT\",\"${LUSTRE_DIR::-1}\",1,.*,.*,[0-9]+,0,0,.*"
        elif [[ $log == *"rbh-report"* ]]; then
            regexp="rbh-report,.*,.*,.*,\"$__rbh_report rbh:$db:$testdb --group-by statx.uid --output sum\(statx.size\)\""
        elif [[ $log == *"rbh-gc"* ]]; then
            expected_column_count=$((expected_column_count + 4))
            regexp="rbh-gc,.*,.*,.*,\"$__rbh_gc rbh:$db:$testdb --sync-time 42\",0,0,0,42"
        else
            error "Invalid command found: '$log'"
        fi

        local comma_count="$(grep -o "," <<< "$log" | wc -l)"
        if (( comma_count + 1 != expected_column_count )); then
            error "Found '$((comma_count + 1))' columns in '$log'," \
                  "expected '$expected_column_count'"
        fi

        regexp="$(echo "$regexp" | sed 's/\//\\\//g')"
        if ! [[ $log =~ $regexp ]]; then
            error "'$log' failed to match with regexp '$regexp'"
        fi

        output="$(echo "$output" | tail -n +2)"
    done
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_oneline test_csv)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
