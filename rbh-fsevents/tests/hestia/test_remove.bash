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

test_remove()
{
    hestia object create blob

    local output=$(invoke_rbh-fsevents)
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    clear_event_feed

    hestia object remove blob

    output=$(invoke_rbh-fsevents)

    local n=$(number_of_events "$output")
    if [[ $n -ne 1 ]]; then
        error "There should only be one event generated, found '$n'"
    fi

    fill_events_array "$output" "delete"

    if [[ ${#events[@]} -ne 1 ]]; then
        error "The sole event in '$output' should be a 'delete'"
    fi

    local remove_id=$(echo "${events[0]}" | cut -d' ' -f5)
    if [ "$object_id" != "$remove_id" ]; then
        error "Remove event should target the same id: remove target" \
              "'$remove_id' != create target '$object_id'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_remove)

run_tests hestia_setup hestia_teardown "${tests[@]}"
