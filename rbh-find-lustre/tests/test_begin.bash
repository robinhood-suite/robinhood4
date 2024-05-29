#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_pool()
{
    local file="test_begin"

    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "$file"
    truncate -s 1M "$file"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -pool blob | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -pool test_pool | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -pool TEST_POOL1 | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -pool test_pool1 | sort |
        difflines "/$test_pool"
    rbh_lfind "rbh:mongo:$testdb" -pool test_pool2 | sort |
        difflines "/$test_pool"
    rbh_lfind "rbh:mongo:$testdb" -pool test_* | sort |
        difflines "/$test_pool"
}

test_i_pool()
{
    local file="pool_file"

    lfs setstripe -E 1M -p "test_pool1" -E 2G -p ""  -E -1 -p "test_pool2" \
        "$file"
    truncate -s 2M "$file"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -i-pool blob | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -i-pool test_pool | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -i-pool TEST_POOL1 | sort |
        difflines "/$test_pool"
    rbh_lfind "rbh:mongo:$testdb" -i-pool test_pool1 | sort |
        difflines "/$test_pool"
    rbh_lfind "rbh:mongo:$testdb" -i-pool tEsT_pOoL2 | sort |
        difflines "/$test_pool"
    rbh_lfind "rbh:mongo:$testdb" -i-pool TEST_* | sort |
        difflines "/$test_pool"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_pool test_i_pool)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
