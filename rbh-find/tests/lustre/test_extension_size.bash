#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    if rbh_find "rbh:$db:$testdb" -extension-size $(echo 2^64 | bc); then
        error "find with an extension size too big should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -extension-size 42blob; then
        error "find with an invalid extension size should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -extension-size invalid; then
        error "find with an invalid extension size should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -extension-size 0; then
        error "find with extension size as 0 should have failed"
    fi
}

test_extension_size()
{
    lfs setstripe --extension-size 64M -c 1 -E -1 file1
    lfs setstripe --extension-size 256M -c 1 -E -1 file2

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -extension-size +20M | sort |
        difflines "/file1" "/file2"
    rbh_find "rbh:$db:$testdb" -extension-size -100M | sort |
        difflines "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -extension-size -100M -extension-size +500M |
        sort | difflines
    rbh_find "rbh:$db:$testdb" -extension-size -100M -or -extension-size +500M |
        sort | difflines "/file1" "/file2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_extension_size)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
