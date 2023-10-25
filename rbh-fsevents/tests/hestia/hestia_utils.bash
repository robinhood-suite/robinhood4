#!/usr/bin/env bash

# This file is part of rbh-fsevents.
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../test_utils.bash

################################################################################
#                                DATABASE UTILS                                #
################################################################################

invoke_rbh-fsevents()
{
    rbh_fsevents src:hestia:$HOME/.cache/hestia/event_feed.yaml -
}

number_of_events()
{
    local n=$(echo "$1" | xargs | awk -F "---" "{ printf NF }")
    echo "$((n - 1))"
}

fill_events_array()
{
    local output="$1"
    local event="$2"

    events=()

    local n=$(number_of_events "$output")
    output=$(echo "$output" | xargs)

    for i in $(seq 1 $((n + 1))); do
        local subevent=$(echo "$output" | awk -F "---" "{ printf \$$i }")

        if echo "$subevent" | grep "$event" > /dev/null; then
            events+=("$subevent")
        fi
    done
}

hestia_setup()
{
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/hestia
    declare -a events
}

hestia_teardown()
{
    rm -rf $HOME/.cache/hestia/
}
