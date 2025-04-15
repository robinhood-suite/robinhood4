#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    if rbh_find "rbh:$db:$testdb" -ost -1; then
        error "find with a negative ost index should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -ost $(echo 2^64 | bc); then
        error "find with an ost index too big should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -ost 42blob; then
        error "find with an invalid ost index should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -ost invalid; then
        error "find with an invalid ost index should have failed"
    fi
}

test_one_match()
{
    local file=test_one_match

    lfs setstripe -i 0 -c 1 $file

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -ost 0 | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -ost 0x0 | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -ost 00 | sort |
        difflines "/$file"
}

test_none()
{
    local file=test_none

    lfs setstripe -i 0 -c 1 $file

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -ost 1 | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -ost 0x1 | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -ost 01 | sort |
        difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_one_match test_none)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
