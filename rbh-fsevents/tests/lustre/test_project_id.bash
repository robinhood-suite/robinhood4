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

test_project_id()
{
    touch "project_1"
    touch "project_2"
    touch "vanilla"

    sudo lfs project -p 1 "project_1"
    sudo lfs project -p 2 "project_2"

    ln -s "vanilla" "vanilla_slink"
    mknod block b 1 2
    mknod fifo p
    mknod char c 1 2

    invoke_rbh-fsevents

    find_attribute '"xattrs.project_id":1' \
                   '"ns.name": "project_1"'
    find_attribute '"xattrs.project_id":2' \
                   '"ns.name": "project_2"'
    find_attribute '"xattrs.project_id":0' \
                   '"ns.name": "vanilla"'

    find_attribute '"ns.name": "vanilla_slink"'
    ! find_attribute '"xattrs.project_id":0' \
                     '"ns.name": "vanilla_slink"'
    find_attribute '"ns.name": "block"'
    ! find_attribute '"xattrs.project_id":0' \
                     '"ns.name": "block"'
    find_attribute '"ns.name": "fifo"'
    ! find_attribute '"xattrs.project_id":0' \
                     '"ns.name": "fifo"'
    find_attribute '"ns.name": "char"'
    ! find_attribute '"xattrs.project_id":0' \
                     '"ns.name": "char"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_project_id)

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
