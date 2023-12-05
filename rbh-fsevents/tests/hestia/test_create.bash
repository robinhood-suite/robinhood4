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

test_create()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)

    if [[ $(number_of_events "$output") != 3 ]]; then
        error "There should be 3 fsevents created (one for link," \
              "inode_xattrs and upsert), found '$(number_of_events "$output")'"
    fi

    if ! echo "$output" | xargs | cut -d'.' -f1 | grep "blob" | \
         grep "link"; then
        error "There should be a link event on the object 'blob' in '$output'"
    fi

    fill_events_array "$output" "link"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'link' event in '$output'"
    fi

    if ! echo "${events[0]}" | grep "blob"; then
        error "The 'link' event should be on the object 'blob'"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    if ! echo "${events[0]}" | grep "tiers" | grep "\[\]"; then
        error "The 'inode_xattr' event should have 'tiers' as an empty array"
    fi

    # We check the changelog time since the enrichment of the times isn't ready
    # yet
    local changelog_time=$(cat ~/.cache/hestia/event_feed.yaml | grep "time" |
                           cut -d' ' -f2)

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be one event 'upsert' in '$output'"
    fi

    if ! echo "${events[0]}" | grep "$changelog_time"; then
        error "One 'upsert' event should set timestamp '$changelog_time'"
    fi

    if ! echo "$output" | grep "$changelog_time"; then
        error "The timestamp '$changelog_time' should appear in '$output'"
    fi

    local n=$(echo "${events[0]}" | grep "$changelog_time" | wc -l)

    if [[ $n != 4 ]]; then
        error "'$changelog_time' should be set for the 4 times, but seen '$n'"
    fi

    if ! echo "${events[0]}" | grep "size" | grep "0"; then
        error "The 'upsert' event should have 'size' set to '0'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_create)

run_tests hestia_setup hestia_teardown ${tests[@]}
