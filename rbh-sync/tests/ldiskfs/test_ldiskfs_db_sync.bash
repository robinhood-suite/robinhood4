#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/../lustre/lustre_utils.bash

rbh_sync_ldiskfs()
{
    rbh_sync "rbh:ldiskfs:/dev/mapper/mds1_flakey" "$1"
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_simple_sync()
{
    mkdir "dir"
    touch "dir/fileA"
    touch "dir/fileB"

    rbh_sync_ldiskfs "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"'$testdir_mds'/'$testdb'/"'
    find_attribute '"ns.xattrs.path":"'$testdir_mds'/'$testdb'/dir/"'
    find_attribute '"ns.xattrs.path":"'$testdir_mds'/'$testdb'/dir/fileA"'
    find_attribute '"ns.xattrs.path":"'$testdir_mds'/'$testdb'/dir/fileB"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_simple_sync)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

lfs setdirstripe -D -i 0 -c 1 $LUSTRE_DIR
tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"
testdir_mds=$(sed -r 's/^\/\w+\/\w+(\/\w+.*)/\1/g' <<< "$tmpdir")

run_tests ${tests[@]}
