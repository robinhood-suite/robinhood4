#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_comp_start()
{
    local file="test_comp_start"

    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "$file"
    truncate -s 1M "$file"

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -comp-start -1k | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -comp-start +1k | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -comp-start 1023k -or -comp-start 1k | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -comp-start -2T | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -comp-start 1M -comp-start 512M | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -comp-start 1M -comp-start 511M | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -not '(' -comp-start 1M -comp-start 511M ')' |
        sort | difflines "/" "/$file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_comp_start)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
