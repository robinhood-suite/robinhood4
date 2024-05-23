#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../test_utils.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

acceptance()
{
    local file="test_file"
    local dump_file="dump_file"

    touch $file
    rm $file

    local changelogs=$(lfs changelog $LUSTRE_MDT | cut -d' ' -f2)
    changelogs=${changelogs:2}

    local output=$(rbh_fsevents --dump "-" src:lustre:"$LUSTRE_MDT" \
                       "rbh:mongo:$testdb")

    while IFS= read -r changelog; do
        local suboutput=$(echo "$output" | grep "$changelog")

        if [[ -z "$suboutput" ]]; then
            error "Changelog '$changelog' should be contained in rbh-fsevents" \
                  "output"
        fi
    done <<< "$changelogs" || exit 1

    rbh_fsevents --dump "$dump_file" src:lustre:"$LUSTRE_MDT" \
                       "rbh:mongo:$testdb"

    local second_output=$(cat $dump_file)

    if [ "$output" != "$second_output" ]; then
        error "Dump to file content differs from dump to stdout"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(acceptance)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests lustre_setup lustre_teardown "${tests[@]}"
