#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../test_utils.bash
. $test_dir/hestia_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_read()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    hestia object put_data --file /etc/hosts blob

    clear_event_feed

    hestia object get_data --file /dev/null blob

    output=$(invoke_rbh-fsevents)
    local n=$(number_of_events "$output")

    if [[ $n != 1 ]]; then
        error "There should be a single event recorded"
    fi

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 1 ]]; then
        error "The sole event in '$output' should be an 'upsert'"
    fi

    local read_id=$(echo "${events[0]}" | grep "id" | cut -d' ' -f3)
    if [ "$object_id" != "$read_id" ]; then
        error "Read event should target the same id: read target" \
              "'$read_id' != create target '$object_id'"
    fi

    local changelog_time=$(cat $HOME/.cache/hestia/event_feed.yaml |
                           grep "time" | cut -d' ' -f2)

    if ! echo "${events[0]}" | grep "$changelog_time"; then
        error "The event should set timestamp '$changelog_time'"
    fi

    local n=$(echo "${events[0]}" | grep "$changelog_time" | wc -l)

    if [[ $n != 1 ]]; then
        error "'$changelog_time' should be set only once, but seen '$n'"
    fi

    if ! echo "${events[0]}" | grep "atime"; then
        error "'$changelog_time' should be set for the access time"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_read)

run_tests hestia_setup hestia_teardown "${tests[@]}"
