#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

generate_commands()
{
    # Ensure there is at least one of each command recorded
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    rbh_find rbh:$db:$testdb -exec ls \; > /dev/null
    rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR \
        src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
    rbh_report rbh:$db:$testdb \
        --group-by "statx.uid" \
        --output "sum(statx.size)" > /dev/null
    rbh_gc rbh:$db:$testdb --sync-time 42

    # Output 16 random ints between 0 and 4
    for i in $(shuf -i 0-4 -r -n 16); do
        case "$i" in
            0)
                rbh_sync rbh:posix:. rbh:$db:$testdb
                ;;
            1)
                rbh_find rbh:$db:$testdb -exec ls \; > /dev/null
                ;;
            2)
                rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR \
                    src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
                ;;
            3)
                rbh_report rbh:$db:$testdb \
                    --group-by "statx.uid" \
                    --output "sum(statx.size)" > /dev/null
                ;;
            4)
                rbh_gc rbh:$db:$testdb --sync-time 42
                ;;
        esac
    done
}

check_expected_stats_value()
{
    local output="$1"
    local to_find="$2"
    local expected="$3"

    local stats_value="$(echo "$output" | grep "$to_find")"
    # Get the string after the first ':'
    stats_value="$(echo "${stats_value#*:}" | xargs)"

    if [ "$stats_value" != "$expected" ]; then
        error "Log value for '$to_find' in '$output' is not '$expected'"
    fi
}

check_common_stats()
{
    local output="$1"
    local command="$2"
    local expected="$3"

    echo "$output" | grep "Start" > /dev/null ||
        error "start_time should have been retrieved, got '$output'"

    echo "$output" | grep "Duration" > /dev/null ||
        error "duration should have been retrieved, got '$output'"

    echo "$output" | grep "End" > /dev/null ||
        error "end_time should have been retrieved, got '$output'"

    local command_line=$(grep 'Command used' <<< "$output" |
                         sed -n "s/.*$command/$command/p")

    if [ "$command_line" != "$expected" ]; then
        error "command lines are not matching, expected '$expected'," \
              "got '$command_line'. Full output is '$output'"
    fi
}

check_timestamps()
{
    local output="$1"
    local start_timestamp="$2"
    local end_timestamp="$3"

    local stats_start="$(echo "$output" | grep "Start of the command")"
    stats_start="${stats_start#*:}"
    stats_start="$(date +%s -d "$stats_start")"

    local stats_end="$(echo "$output" | grep "End of the command")"
    stats_end="${stats_end#*:}"
    stats_end="$(date +%s -d "$stats_end")"

    if (( $stats_start > $stats_end )); then
        error "Command start timestamp ($stats_start) is after command end timestamp ($stats_end)"
    fi

    # We can't really verify that the start timestamp obtained before the
    # command and the end timestamp obtained after are correct compared to the
    # timestamp retrieved from the stats, as there may be some delay between them.
    # So the only thing we can check is that the stats timestamps SEEM valid
    # compared to the given timestamp
    if (( $start_timestamp > $stats_start )); then
        error "Given start timestamp ($start_timestamp) is after command start timestamp ($stats_start)"
    fi

    if (( $end_timestamp < $stats_end )); then
        error "Given end timestamp ($end_timestamp) is before command end timestamp ($stats_end)"
    fi

    local stats_duration="$(echo "$output" | grep "Duration" | cut -d':' -f2 |
                          xargs | tr -d -c 0-9)"

    if (( $stats_end - $stats_start != $stats_duration )); then
        error "Log duration ($stats_duration) is not the difference between stats end ($stats_end) and stats start ($stats_start)"
    fi
}

check_common_timestamps()
{
    local stats_type="$1"
    local full_command=$2

    local start_timestamp
    local end_timestamp
    local output

    start_timestamp="$(date -u +%s)"
    eval $full_command
    end_timestamp="$(date -u +%s)"
    output=$(rbh_stats "rbh:$db:$testdb" --tool $stats_type -n 1)
    check_timestamps "$output" "$start_timestamp" "$end_timestamp"

    sleep 1

    start_timestamp="$(date +%s)"
    eval $full_command
    end_timestamp="$(date +%s)"
    output=$(rbh_stats "rbh:$db:$testdb" --tool $stats_type -n 1)
    check_timestamps "$output" "$start_timestamp" "$end_timestamp"
}
