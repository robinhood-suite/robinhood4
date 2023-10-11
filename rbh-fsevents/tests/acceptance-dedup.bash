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
    if grep "!xattrs" /tmp/output.yaml; then
        error "No xattrs event should be found after deduplication"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(creat_delete)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
