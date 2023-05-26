#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

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
    for i in $(seq 1 ${other_mdt_user:2}); do
        lfs changelog_clear "$other_mdt" "cl$i" 0
    done

    mkdir $entry

    invoke_rbh-fsevents
    clear_changelogs "$LUSTRE_MDT" "$userid"

    lfs migrate -m 1 $entry
    # This will be done more properly in a next patch
    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$other_mdt" \
        "rbh:mongo:$testdb"
    clear_changelogs "$other_mdt" "$other_mdt_user"

    lfs migrate -m 0 $entry

    find_attribute '"xattrs.mdt_index": 1' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.mdt_count": 1' '"ns.name":"'$entry'"'

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    verify_statx $entry
    find_attribute '"xattrs.mdt_index": 0' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.mdt_count": 1' '"ns.name":"'$entry'"'
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

run_tests ${tests[@]}
