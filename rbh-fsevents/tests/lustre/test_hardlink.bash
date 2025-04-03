#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

create_entry()
{
    touch "$1.tmp"
    ln "$1.tmp" "$1"
}

create_filled_entry()
{
    echo "blob" | dd oflag=direct of="$1.tmp"
    ln "$1.tmp" "$1"
}

test_create_hardlink()
{
    local entry="test_entry"
    create_entry $entry

    invoke_rbh-fsevents

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    count=$((count - 1))
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    find_attribute "\"ns.name\":\"$entry.tmp\"" "\"ns.name\":\"$entry\""
    find_attribute '"xattrs.nb_children": '"$NB_ENTRY"''
    verify_statx "$entry"
    verify_statx "$entry.tmp"
    verify_lustre "$entry"
}

################################################################################
#                                     MAIN                                     #
################################################################################

NB_ENTRY=2
source $test_dir/test_create_inode.bash

declare -a tests=(test_create_hardlink test_create_two_entries)

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
