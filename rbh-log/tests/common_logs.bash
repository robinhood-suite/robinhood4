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
