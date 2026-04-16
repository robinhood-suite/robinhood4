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

test_fid()
{
    mkdir blob
    touch blob/blob

    local root_fid="$(lfs path2fid .)"
    local blob_fid="$(lfs path2fid blob)"
    local blob_blob_fid="$(lfs path2fid blob/blob)"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%F'\n" | sort |
        difflines "/: '$root_fid'" "/blob/blob: '$blob_blob_fid'" \
                  "/blob: '$blob_fid'"

}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_fid)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
