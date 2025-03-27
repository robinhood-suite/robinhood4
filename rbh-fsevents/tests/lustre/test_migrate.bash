#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

mdt_count=$(lfs mdts | wc -l)
if [[ $mdt_count -lt 2 ]]; then
    exit 77
fi

################################################################################
#                                    TESTS                                     #
################################################################################

test_migrate()
{
    local entry="test_entry"
    local other_mdt="lustre-MDT0001"
    # This will be done more properly in a next patch
    local other_mdt_user="$(lctl --device "$other_mdt" changelog_register |
                            cut -d "'" -f2)"
    # Clear changelogs from previous tests
    lfs changelog_clear "$LUSTRE_MDT" "$userid" 0
    lfs changelog_clear "$other_mdt" "$other_mdt_user" 0

    mkdir $entry

    invoke_rbh-fsevents
    clear_changelogs "$LUSTRE_MDT" "$userid"

    lfs migrate -m 1 $entry
    # This will be done more properly in a next patch
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" src:lustre:"$other_mdt" \
        "rbh:$db:$testdb"
    clear_changelogs "$other_mdt" "$other_mdt_user"

    lfs migrate -m 0 $entry

    find_attribute '"xattrs.mdt_index": 1' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.mdt_count": 1' '"ns.name":"'$entry'"'

    invoke_rbh-fsevents

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    verify_statx $entry
    find_attribute '"xattrs.mdt_index": 0' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.mdt_count": 1' '"ns.name":"'$entry'"'

    lctl --device "$other_mdt" changelog_deregister "$other_mdt_user"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_migrate)

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
