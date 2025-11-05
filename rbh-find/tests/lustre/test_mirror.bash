#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_mirror_count()
{
    touch file
    lfs mirror create -N2 file2
    lfs mirror create -N3 file3

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -mirror-count 2 | sort |
        difflines "/file2"
    rbh_find "rbh:$db:$testdb" -mirror-count +2 | sort |
        difflines "/file3"
    rbh_find "rbh:$db:$testdb" -mirror-count -2 | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -not -mirror-count 3 | sort |
        difflines "/" "/file" "/file2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_mirror_count)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
