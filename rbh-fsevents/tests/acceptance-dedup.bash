#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

creat_delete()
{
    # Clear remaining changelogs
    clear_changelogs "$LUSTRE_MDT" "$userid"

    touch file
    rm file

    rbh_fsevents --batch-size 0 --enrich rbh:lustre:"$LUSTRE_DIR" \
        --lustre "$LUSTRE_MDT" - > /tmp/output.yaml
    if ! grep "!link" /tmp/output.yaml; then
        error "Without deduplication, a link event should be emitted"
    fi
    if ! grep "!delete" /tmp/output.yaml; then
        error "Without deduplication, a delete event should be emitted"
    fi

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        --lustre "$LUSTRE_MDT" - > /tmp/output.yaml
    if grep "!link" /tmp/output.yaml; then
        error "No link event should be found after deduplication"
    fi
    if grep "!delete" /tmp/output.yaml; then
        error "No delete event should be found after deduplication"
    fi
}

test_flush_size()
{
    touch file
    rm file

    # A flush size of 1 means that we will process events one by one so no
    # deduplication will happen.
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb" --batch-size 100 --flush-size 1 > output.yaml

    # TODO make sure that we have a link and unlink and upsert...
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(creat_delete test_flush_size)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
