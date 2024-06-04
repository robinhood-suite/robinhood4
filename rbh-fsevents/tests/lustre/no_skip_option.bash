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

no_skip_option()
{
    local file="test_file"

    lfs setstripe -c 1 $file

    local non_enriched_output=$(rbh_fsevents src:lustre:"$LUSTRE_MDT" -)

    rm $file

    echo "$non_enriched_output" |
        rbh_fsevents --no-skip --enrich rbh:lustre:"$LUSTRE_DIR" \
        - rbh:mongo:$testdb &&
        error "fsevents with an error at enrich and the 'no-skip' flag should" \
              "have failed"

    echo "$non_enriched_output" |
        rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        - rbh:mongo:$testdb ||
        error "fsevents with a skipped error at enrich should have succeeded"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(no_skip_option)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests lustre_setup lustre_teardown "${tests[@]}"
