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

invoke_rbh_fsevents()
{
    rbh_fsevents src:hestia:$HOME/.cache/hestia/event_feed.yaml "$*"
}

number_of_events()
{
    local n=$(echo "$1" | xargs | awk -F "---" "{ printf NF }")
    echo "$((n - 1))"
}

fill_events_array()
{
    local entry="$1"
    local event="$2"
    local arr

    events=()

    entry=$(echo "$entry" | sed "s/---/§/")
    IFS='§' read -d '' -a arr <<< "$entry" || true

    local n=${#arr[@]}

    for i in $(seq 0 $((n - 1))); do
        if echo "${arr[$i]}" | grep "$event" > /dev/null; then
            events+=("${arr[$i]}")
        fi
    done
}

clear_event_feed()
{
    rm $HOME/.cache/hestia/event_feed.yaml
}

hestia_setup()
{
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib64/hestia
    declare -a events
}

hestia_teardown()
{
    rm -rf $HOME/.cache/hestia/
}

dump_feed()
{
    cat $HOME/.cache/hestia/event_feed.yaml
}

hestia_get_attr()
{
    local obj="$1"
    local attr="$2"

    echo "$(hestia object --verbosity 1 read "$obj" | grep "$attr" | head -n 1 |
            cut -d':' -f2 | cut -d',' -f1 | xargs)"
}

find_time_attribute()
{
    local category="$1"
    local time="$2"
    local object="$3"

    find_attribute '"statx.'$category'.sec":NumberLong("'$time'")' \
                   '"ns.name":"'$object'"'
}
