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

    if [[ $(number_of_events "$output") -ne 4 ]]; then
        error "There should be 4 fsevents created (one for link," \
              "inode_xattrs, upsert of statx and upsert of enrichment tag)," \
              "found '$(number_of_events "$output")'"
    fi

    if ! echo "$output" | xargs | cut -d'.' -f1 | grep "blob" | \
         grep "link"; then
        error "There should be a link event on the object 'blob' in '$output'"
    fi

    fill_events_array "$output" "link"

    if [[ ${#events[@]} -ne 1 ]]; then
        error "There should be a single 'link' event in '$output'"
    fi

    if ! echo "${events[0]}" | grep "blob"; then
        error "The 'link' event should be on the object 'blob'"
    fi

    local hestia_id=$(hestia object read blob | jq -r ".dataset.id")

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} -ne 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    if ! echo "${events[0]}" | grep "$hestia_id"; then
        error "The 'inode_xattr' event should set '$hestia_id'"
    fi

    # We check the changelog time since the enrichment of the times isn't ready
    # yet
    local changelog_time=$(cat ~/.cache/hestia/event_feed.yaml | grep "time" |
                           cut -d' ' -f2)

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} -ne 2 ]]; then
        error "There should be two events 'upsert' in '$output'"
    fi

    if ! echo "${events[0]}" | grep "$changelog_time"; then
        error "One 'upsert' event should set timestamp '$changelog_time'"
    fi

    if ! echo "$output" | grep "$changelog_time"; then
        error "The timestamp '$changelog_time' should appear in '$output'"
    fi

    local n=$(echo "${events[0]}" | awk -F "$changelog_time" "{ printf NF }")

    if [[ $((n - 1)) -ne 4 ]]; then
        error "'$changelog_time' should be set for the 4 times, but seen '$n'"
    fi

    if ! echo "${events[1]}" | grep "rbh-fsevents"; then
        error "One 'upsert' event should set the enrichment tag"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_create)

run_tests hestia_setup hestia_teardown ${tests[@]}
