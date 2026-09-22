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

check_over_output_is_correct()
{
    local format="$1"

    local output=$(rbh_log "rbh:$db:$testdb" -n 21 $format)
    local tmp_output=$(rbh_log "rbh:$db:$testdb" -n 30 $format)

    if [ "$output" != "$tmp_output" ]; then
        error "Outputted oneline logs should have been the same, got '$output' and '$tmp_output'"
    fi

}

test_oneline()
{
    generate_commands
    check_over_output_is_correct --oneline

    local output=$(rbh_log "rbh:$db:$testdb" -n 21 --oneline)

    while [ ! -z "$output" ]; do
        local log="$(echo "$output" | head -n 1)"

        local expected_output=("Start" "Duration")
        if [[ $log == *"rbh-sync"* ]]; then
            expected_output+=("Entries converted" "Entries seen")
        elif [[ $log == *"rbh-find"* ]]; then
            expected_output+=("Entries post-filtering")
        elif [[ $log == *"rbh-fsevents"* ]]; then
            expected_output+=("Changelog read" "Read/dedup time"
                              "Enrich/update time")
        elif [[ $log == *"rbh-report"* ]]; then
            # Nothing to add here
            echo "blob"
        elif [[ $log == *"rbh-gc"* ]]; then
            expected_output+=("Entries deleted" "Entries seen")
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
    check_over_output_is_correct --csv

    local output=$(rbh_log "rbh:$db:$testdb" -n 21 --csv)

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

test_json()
{
    generate_commands
    check_over_output_is_correct --json

    local output=$(rbh_log "rbh:$db:$testdb" -n 21 --json)

    while [ ! -z "$output" ]; do
        local command="$(echo "$output" | tail -n +2 | head -n 1)"
        # Start a line counter to check one log has the correct format. There
        # are always one line with the starting '{', another with the command,
        # and two at the end with ending '}'
        local expected_line_count=4
        local regexp

        if [[ $command == *"rbh-sync"* ]]; then
            expected_line_count=$((expected_line_count + 8))
            regexp=(".*" ".*" ".*"
                    "\"$__rbh_sync rbh:posix:. rbh:$db:$testdb\""
                    "\"$PWD\"" "1" "0" "1")
        elif [[ $command == *"rbh-find"* ]]; then
            expected_line_count=$((expected_line_count + 6))
            regexp=(".*" ".*" ".*"
                    "\"$__rbh_find rbh:$db:$testdb -exec ls ;\""
                    "[0-9]+" "[0-9]+")
        elif [[ $command == *"rbh-fsevents"* ]]; then
            expected_line_count=$((expected_line_count + 13))
            regexp=(".*" ".*" ".*"
                    "\"$__rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb\""
                    "\"$LUSTRE_MDT\"" "\"${LUSTRE_DIR::-1}\"" "1"
                    ".*" ".*" "[0-9]+" "0" "0" ".*")
        elif [[ $command == *"rbh-report"* ]]; then
            expected_line_count=$((expected_line_count + 4))
            regexp=(".*" ".*" ".*"
                    "\"$__rbh_report rbh:$db:$testdb --group-by statx.uid --output sum\(statx.size\)\"")
        elif [[ $command == *"rbh-gc"* ]]; then
            expected_line_count=$((expected_line_count + 8))
            regexp=(".*" ".*" ".*"
                    "\"$__rbh_gc rbh:$db:$testdb --sync-time 42\""
                    "0" "0" "0" "42")
        else
            error "Invalid command found: '$command'"
        fi

        local log="$(echo "$output" | head -n $expected_line_count)"
        if ! jq -e . >/dev/null 2>&1 <<<"$log"; then
            error "'$log' is not a valid JSON document"
        fi

        # Skip over the two initial lines + the last two
        log="$(echo "$log" | tail -n +3 | head -n -2)"
        local counter=0

        while IFS= read -r line; do
            local line_regexp="$(echo "${regexp[$counter]}" |
                                 sed 's/\//\\\//g')"
            if ! [[ $line =~ $line_regexp ]]; then
                error "'$line' failed to match with regexp '$line_regexp'"
            fi

            counter=$((counter + 1))
        done <<< "$log"

        output="$(echo "$output" | tail -n +$((expected_line_count + 1)))"
    done
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_oneline test_csv test_json)

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
