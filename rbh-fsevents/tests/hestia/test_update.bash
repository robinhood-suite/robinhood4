#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
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

    if [ -z "$n" ] || [[ $n != 3 ]]; then
        error "'$changelog_time' should be set twice, but seen '$n'"
    fi

    if ! echo "${events[0]}" | grep "ctime"; then
        error "'$changelog_time' should be set for the access time"
    fi
}

check_size()
{
    local output="$1"
    local size="$2"

    if ! echo "$output" | grep "size" | grep "$size"; then
        error "The 'upsert' event should have 'size' set to '$size'"
    fi
}

test_update_xattrs()
{
    hestia object create blob

    local output=$(invoke_rbh_fsevents "-")
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    clear_event_feed

    hestia metadata update --id_fmt=parent_id --input_fmt=key_value blob <<< \
        data.my_key=my_value

    output=$(invoke_rbh_fsevents "-")

    local n=$(number_of_events "$output")
    if [[ $n != 2 ]]; then
        error "There should be 2 events generated"
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

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be 1 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
    check_size "${events[0]}" 0
}

check_tier()
{
    local output="$1"
    local tier="$2"
    local size="$3"

    if ! echo "$output" | grep "tiers"; then
        error "'tiers' should be present in '$output'"
    fi

    if ! echo "$output" | grep "index" | grep "$tier"; then
        error "Data should be on tier with index '$tier'"
    fi

    if ! echo "$output" | grep "length" | grep "$size"; then
        error "The data on '$tier' should be of length '$size'"
    fi

    entries=$(echo "$output" | grep "index" | grep "$tier")
    entries_lines=$(echo "$entries" | wc -l)
    if [[ $entries_lines != 1 ]]; then
        error "The data should have been set on '$tier' only once"
    fi
}

test_update_data()
{
    hestia object create blob

    local output=$(invoke_rbh_fsevents "-")
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    clear_event_feed

    hestia object put_data --file /etc/hosts blob

    output=$(invoke_rbh_fsevents "-")

    local n=$(number_of_events "$output")
    if [[ $n != 2 ]]; then
        error "There should be 2 events generated"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    check_id "${events[0]}" "$object_id"

    check_tier "${events[0]}" "0" "$(stat -c %s /etc/hosts)"

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be 1 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
    check_size "${events[0]}" "$(stat -c %s /etc/hosts)"
}

test_update_copy()
{
    hestia object create blob

    local output=$(invoke_rbh_fsevents "-")
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    hestia object put_data --file /etc/hosts blob

    clear_event_feed

    hestia object copy_data blob --source 0 --target 1

    output=$(invoke_rbh_fsevents "-")

    local n=$(number_of_events "$output")
    if [[ $n != 2 ]]; then
        error "There should be 2 events generated"
    fi

    fill_events_array "$output" "inode_xattr"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be a single 'inode_xattr' event in '$output'"
    fi

    check_id "${events[0]}" "$object_id"

    check_tier "${events[0]}" "0" "$(stat -c %s /etc/hosts)"
    check_tier "${events[0]}" "1" "$(stat -c %s /etc/hosts)"

    fill_events_array "$output" "upsert"

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be 1 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
    check_size "${events[0]}" "$(stat -c %s /etc/hosts)"
}

test_update_release()
{
    hestia object create blob

    local output=$(invoke_rbh_fsevents "-")
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    hestia object put_data --file /etc/hosts blob
    hestia object copy_data blob --source 0 --target 1

    clear_event_feed

    hestia object release_data blob --tier 0

    output=$(invoke_rbh_fsevents "-")

    local n=$(number_of_events "$output")
    if [[ $n != 2 ]]; then
        error "There should be 2 events generated"
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

    if [[ ${#events[@]} != 1 ]]; then
        error "There should be 1 'upsert' events in '$output'"
    fi

    check_ctime "${events[0]}"
    check_size "${events[0]}" "$(stat -c %s /etc/hosts)"
}

test_update_xattrs_to_db()
{
    local obj=$(hestia object --verbosity 1 create blob)

    clear_event_feed

    hestia metadata update --id_fmt=parent_id --input_fmt=key_value $obj <<< \
        data.my_key=my_value

    invoke_rbh_fsevents "rbh:$db:$testdb"

    find_attribute '"xattrs.tiers": { $size: 0 }'
    find_attribute '"xattrs.user_metadata": { "my_key": "my_value" }'

    local time=$(hestia_get_attr "$obj" "metadata_modified_time")
    find_time_attribute "ctime" "$time"

    time=$(hestia_get_attr "$obj" "content_modified_time")
    find_time_attribute "mtime" "$time"

    find_attribute '"statx.size": 0'
}

test_update_data_to_db()
{
    local obj=$(hestia object --verbosity 1 create blob)

    clear_event_feed

    hestia object put_data --file /etc/hosts $obj

    invoke_rbh_fsevents "rbh:$db:$testdb"

    local size="$(stat -c %s /etc/hosts)"

    find_attribute '"xattrs.tiers": { $size: 1 }'
    find_attribute '"xattrs.tiers.extents.length": "'$size'"'
    find_attribute '"xattrs.tiers.extents.offset": "0"'
    find_attribute '"xattrs.tiers.index": "0"'
    find_attribute '"xattrs.user_metadata": { }'

    local time=$(hestia_get_attr "$obj" "content_modified_time")

    find_time_attribute "ctime" "$time"

    find_attribute '"statx.size": NumberLong('$size')'
}

test_update_copy_to_db()
{
    local obj=$(hestia object --verbosity 1 create blob)

    hestia object put_data --file /etc/hosts $obj

    clear_event_feed

    hestia object copy_data $obj --source 0 --target 1

    invoke_rbh_fsevents "rbh:$db:$testdb"

    local size="$(stat -c %s /etc/hosts)"

    find_attribute '"xattrs.tiers": { $size: 2 }'
    find_attribute '"xattrs.tiers.extents.length": "'$size'"' \
                   '"xattrs.tiers.index": "1"'
    find_attribute '"xattrs.tiers.extents.offset": "0"' \
                   '"xattrs.tiers.index": "1"'
    find_attribute '"xattrs.tiers.extents.length": "'$size'"' \
                   '"xattrs.tiers.index": "0"'
    find_attribute '"xattrs.tiers.extents.offset": "0"' \
                   '"xattrs.tiers.index": "0"'
    find_attribute '"xattrs.user_metadata": { }'

    local time=$(hestia_get_attr "$obj" "content_modified_time")

    find_time_attribute "ctime" "$time"

    find_attribute '"statx.size": NumberLong('$size')'
}

test_update_release_to_db()
{
    local obj=$(hestia object --verbosity 1 create blob)

    hestia object put_data --file /etc/hosts $obj
    hestia object copy_data $obj --source 0 --target 1

    clear_event_feed

    hestia object release_data $obj --tier 0

    invoke_rbh_fsevents "rbh:$db:$testdb"

    local size="$(stat -c %s /etc/hosts)"

    find_attribute '"xattrs.tiers": { $size: 1 }'
    find_attribute '"xattrs.tiers.extents.length": "'$size'"'
    find_attribute '"xattrs.tiers.extents.offset": "0"'
    find_attribute '"xattrs.tiers.index": "1"'
    find_attribute '"xattrs.user_metadata": { }'

    local time=$(hestia_get_attr "$obj" "content_modified_time")

    find_time_attribute "ctime" "$time"

    find_attribute '"statx.size": NumberLong('$size')'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_update_xattrs test_update_data test_update_copy
                  test_update_release test_update_xattrs_to_db
                  test_update_data_to_db test_update_copy_to_db
                  test_update_release_to_db)

sub_setup=hestia_setup
sub_teardown=hestia_teardown
run_tests "${tests[@]}"
