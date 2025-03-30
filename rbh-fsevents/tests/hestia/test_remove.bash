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

test_remove()
{
    hestia object create blob

    local output=$(invoke_rbh_fsevents "-")
    local object_id=$(echo "$output" | grep "id" | xargs | cut -d' ' -f3)

    clear_event_feed

    hestia object remove blob

    output=$(invoke_rbh_fsevents "-")

    local n=$(number_of_events "$output")
    if [[ $n != 1 ]]; then
        error "There should only be one event generated, found '$n'"
    fi

    fill_events_array "$output" "delete"

    if [[ ${#events[@]} != 1 ]]; then
        error "The sole event in '$output' should be a 'delete'"
    fi

    local remove_id=$(echo "${events[0]}" | grep "id" | cut -d' ' -f3)
    if [ "$object_id" != "$remove_id" ]; then
        error "Remove event should target the same id: remove target" \
              "'$remove_id' != create target '$object_id'"
    fi
}

test_remove_to_db()
{
    local obj=$(hestia object --verbosity 1 create blob)
    invoke_rbh_fsevents "rbh:$db:$testdb"

    clear_event_feed

    hestia object remove "$obj"

    find_attribute '"ns.name":"'$obj'"'
    invoke_rbh_fsevents "rbh:$db:$testdb"

    local count=$(do_db count "$testdb")
    if (( $count != 0 )); then
        error "Remove event should have deleted sole record in the DB"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_remove test_remove_to_db)

sub_setup=hestia_setup
sub_teardown=hestia_teardown
run_tests "${tests[@]}"
