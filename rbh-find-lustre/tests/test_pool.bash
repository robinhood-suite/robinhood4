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

test_pool()
{
    local file="pool_file"
    local file2="pool_file2"

    lfs setstripe -E 1M -p "test_pool1" -E 2G -p ""  -E -1 -p "test_pool2" \
        "$file"
    truncate -s 2M "$file"
    lfs setstripe -p "test_pool3" "$file2"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb" -pool blob | sort |
        difflines
    rbh_lfind "rbh:$db:$testdb" -pool test_pool | sort |
        difflines
    rbh_lfind "rbh:$db:$testdb" -pool TEST_POOL1 | sort |
        difflines
    rbh_lfind "rbh:$db:$testdb" -pool test_pool1 | sort |
        difflines "/$file"
    rbh_lfind "rbh:$db:$testdb" -pool test_pool2 | sort |
        difflines "/$file"
    rbh_lfind "rbh:$db:$testdb" -pool test_* | sort |
        difflines "/$file" "/$file2"
}

test_ipool()
{
    local file="pool_file"
    local file2="pool_file2"

    lfs setstripe -E 1M -p "test_pool1" -E 2G -p ""  -E -1 -p "test_pool2" \
        "$file"
    truncate -s 2M "$file"
    lfs setstripe -p "test_pool3" "$file2"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_lfind "rbh:$db:$testdb" -ipool blob | sort |
        difflines
    rbh_lfind "rbh:$db:$testdb" -ipool test_pool | sort |
        difflines
    rbh_lfind "rbh:$db:$testdb" -ipool TEST_POOL1 | sort |
        difflines "/$file"
    rbh_lfind "rbh:$db:$testdb" -ipool test_pool1 | sort |
        difflines "/$file"
    rbh_lfind "rbh:$db:$testdb" -ipool tEsT_pOoL2 | sort |
        difflines "/$file"
    rbh_lfind "rbh:$db:$testdb" -ipool TEST_* | sort |
        difflines "/$file" "/$file2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_pool test_ipool)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
