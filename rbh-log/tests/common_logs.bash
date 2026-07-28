#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

check_expected_log_value()
{
    local output="$1"
    local to_find="$2"
    local expected="$3"

    local log_value="$(echo "$output" | grep "$to_find")"
    # Get the string after the first ':'
    log_value="$(echo "${log_value#*:}" | xargs)"

    if [ "$log_value" != "$expected" ]; then
        error "Log value for '$to_find' in '$output' is not '$expected'"
    fi
}

check_common_logs()
{
    local output="$1"
    local command="$2"
    local expected="$3"

    echo "$output" | grep "Start" > /dev/null ||
        error "start_time should have been retrieved"

    echo "$output" | grep "Duration" > /dev/null ||
        error "duration should have been retrieved"

    echo "$output" | grep "End" > /dev/null ||
        error "end_time should have been retrieved"

    local command_line=$(grep 'Command used' <<< "$output" |
                         sed -n "s/.*$command/$command/p")

    if [ "$command_line" != "$expected" ]; then
        error "command lines are not matching, expected '$expected', got '$command_line'"
    fi
}

check_timestamps()
{
    local output="$1"
    local start_timestemp="$2"
    local end_timestemp="$3"

    local log_start="$(echo "$output" | grep "Start")"
    log_start="${log_start#*:}"
    log_start="$(date +%s -d "$log_start")"

    local log_end="$(echo "$output" | grep "End")"
    log_end="${log_end#*:}"
    log_end="$(date +%s -d "$log_end")"

    if (( $log_start > $log_end )); then
        error "Command start timestamp ($log_start) is after command end timestamp ($log_end)"
    fi

    # We can't really verify that the start timestamp obtained before the
    # command and the end timestamp obtained after are correct compared to the
    # timestamp retrieved from the log, as there may be some delay between them.
    # So the only thing we can check is that the log timestamps SEEM valid
    # compared to the given timestamp
    if (( $start_timestamp > $log_start )); then
        error "Given start timestamp ($start_timestamp) is after command start timestamp ($log_start)"
    fi

    if (( $end_timestamp < $log_end )); then
        error "Given end timestamp ($end_timestamp) is after command end timestamp ($log_end)"
    fi

    local log_duration="$(echo "$output" | grep "Duration" | cut -d':' -f2 |
                          xargs))"
    if (( $log_end - $log_start != $log_duration )); then
        error "Log duration ($log_duration) is not the difference between log end ($log_end) and log start ($log_start)"
    fi
}
