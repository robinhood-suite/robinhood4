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

test_comp_start()
{
    local file="test_comp_start"

    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "$file"
    truncate -s 1M "$file"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    lfs getstripe -v "$file"

    mongo $testdb --eval "db.entries.find()"

    rbh_lfind "rbh:mongo:$testdb" -comp-start -1k | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -comp-start +1k | sort |
        difflines "/$file"
    # XXX: this test doesn't work because we have an issue with how we handle
    # filters on arrays in the Mongo database
#    rbh_lfind "rbh:mongo:$testdb" -comp-start 1023k -or -comp-start 1k | sort |
#        difflines
    rbh_lfind "rbh:mongo:$testdb" -comp-start -2T | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -comp-start 1M -comp-start 512M | sort |
        difflines "/$file"
    # XXX: for the same reason as above, this test shouldn't output anything,
    # but it currently doesn't
#    rbh_lfind "rbh:mongo:$testdb" -comp-start 1M -comp-start 511M | sort |
#        difflines
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
