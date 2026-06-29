#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_selinux()
{
    local entry="test_file"

    touch "$entry"
    chcon -l "s2:c2,c5-s8:c2,c5" "$entry"

    invoke_rbh-fsevents lustre-selinux

    find_attribute '"xattrs.selinux.context":{$exists:true}' \
        '"ns.name":"'$entry'"'

    find_attribute '"xattrs.selinux.range":"s2:c2,c5-s8:c2,c5"' \
        '"ns.name":"'$entry'"'

    find_attribute '"xattrs.selinux.high_sens":8' \
        '"ns.name":"'$entry'"'

    chcon -l "s2:c2,c5-s12:c2,c5,c12,c14" "$entry"

    invoke_rbh-fsevents lustre-selinux

    find_attribute '"xattrs.selinux.range":"s2:c2,c5-s12:c2,c5,c12,c14"' \
        '"ns.name":"'$entry'"'

    find_attribute '"xattrs.selinux.high_sens":12' \
        '"ns.name":"'$entry'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_selinux)

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
