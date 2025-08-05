#!/usr/bin/env bash

# This file is part of rbh-undelete.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/../../rbh-fsevents/tests/lustre/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_simple_undelete()
{
    local entry="entry"
    local cp_entry="cp_entry"

    echo "test_content" > "$entry"

    local path=$(realpath "$entry")

    cp $entry $cp_entry

    archive_file $entry

    invoke_rbh-fsevents

    rm $entry

    invoke_rbh-fsevents

    rbh_undelete "rbh:$db:$testdb" "rbh:lustre:$path" --restore

    if [ ! -f "$entry" ]; then
        error "rbh-undelete failed to restore $entry"
    fi

    hsm_restore_file $entry

    diff "$entry" "$cp_entry" || error "Content restored is not matching"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_simple_undelete)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
