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

check_id()
{
    local output="$1"
    local object_id="$2"

    local update_id=$(echo "$output" | grep "id" | cut -d' ' -f3)
    if [ "$object_id" != "$update_id" ]; then
        error "Update event should target the same id: update target" \
              "'$update_id' != create target '$object_id'"
    fi
}

check_ctime()
{
    local output="$1"

    local changelog_time=$(cat $HOME/.cache/hestia/event_feed.yaml |
                           grep "time" | cut -d' ' -f2)

    if ! echo "$output" | grep "$changelog_time"; then
        error "The event should set timestamp '$changelog_time'"
    fi

    local n=$(echo "${events[0]}" | grep "$changelog_time" | wc -l)

    if [ -z "$n" ] || [[ $n != 1 ]]; then
        error "'$changelog_time' should be set only once, but seen '$n'"
    fi

    if ! echo "${events[0]}" | grep "ctime"; then
        error "'$changelog_time' should be set for the access time"
    fi
}

test_update_xattrs()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    clear_event_feed

    hestia metadata update --id_fmt=parent_id --input_fmt=key_value blob <<< \
        data.key,value

    output=$(invoke_rbh-fsevents)

    local n=$(number_of_events "$output")
    if [[ $n != 3 ]]; then
        error "There should be 3 events generated"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    check_id "${events[0]}" "$object_id"

    if ! echo "${events[0]}" | grep "data"; then
        error "The word 'data' should be present in '${events[0]}'"
    fi

    entries=$(echo "${events[0]}" | grep "data")
    entries_lines=$(echo "$entries" | wc -l)
    if [[ $entries_lines != 1 ]]; then
        error "The 'data' set should be present only once"
    fi

    if ! echo "${events[0]}" | grep "key" | grep "value"; then
        error "The xattrs couple 'key' = 'value' should be present in '$output'"
    fi

    entries=$(echo "${events[0]}" | grep "key" | grep "value")
    entries_lines=$(echo "$entries" | wc -l)
    if [[ $entries_lines != 1 ]]; then
        error "The xattrs couple 'key' = 'value' should be set only once"
    fi

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 2 ]]; then
        error "There should be 2 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
}

check_tier()
{
    local output="$1"
    local tier="$2"
    local size="$3"

    if ! echo "$output" | grep "tiers"; then
        error "'tiers' should be present in '$output'"
    fi

    if ! echo "$output" | grep "name" | grep "$tier"; then
        error "Data should be on tier '$tier'"
    fi

    if ! echo "$output" | grep "size" | grep "$size"; then
        error "The data on '$tier' should be of length '$size'"
    fi

    entries=$(echo "$output" | grep "name" | grep "$tier")
    entries_lines=$(echo "$entries" | wc -l)
    if [[ $entries_lines != 1 ]]; then
        error "The data should have been set on '$tier' only once"
    fi
}

test_update_data()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    clear_event_feed

    hestia object put_data --file /etc/hosts blob

    output=$(invoke_rbh-fsevents)

    local n=$(number_of_events "$output")
    if [[ $n != 3 ]]; then
        error "There should be 3 events generated"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    check_id "${events[0]}" "$object_id"

    check_tier "${events[0]}" "0" "$(stat -c %s /etc/hosts)"

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 2 ]]; then
        error "There should be 2 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
}

test_update_copy()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    hestia object put_data --file /etc/hosts blob

    clear_event_feed

    hestia object copy_data blob --source 0 --target 1

    output=$(invoke_rbh-fsevents)

    local n=$(number_of_events "$output")
    if [[ $n != 3 ]]; then
        error "There should be 3 events generated"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    check_id "${events[0]}" "$object_id"

    check_tier "${events[0]}" "0" "$(stat -c %s /etc/hosts)"
    check_tier "${events[0]}" "1" "$(stat -c %s /etc/hosts)"

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 2 ]]; then
        error "There should be 2 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
}

test_update_release()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    hestia object put_data --file /etc/hosts blob
    hestia object copy_data blob --source 0 --target 1

    clear_event_feed

    hestia object release_data blob --tier 0

    output=$(invoke_rbh-fsevents)

    local n=$(number_of_events "$output")
    if [[ $n != 3 ]]; then
        error "There should be 3 events generated"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    check_id "${events[0]}" "$object_id"

    check_tier "${events[0]}" "1" "$(stat -c %s /etc/hosts)"

    if echo "${events[0]}" | grep "tier_0"; then
        error "There should be no data on 'tier_0'"
    fi

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 2 ]]; then
        error "There should be 2 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_update_xattrs test_update_data test_update_copy
                  test_update_release)

run_tests hestia_setup hestia_teardown ${tests[@]}
