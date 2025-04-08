#!/usr/bin/env bash

# This file is part of rbh-info.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_lustre_source()
{
    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 3)); then
        error "Three backends should have been registered"
    fi

    echo "$output" | grep "posix" ||
        error "The POSIX backend should have been registered"

    echo "$output" | grep "lustre" ||
        error "The Lustre backend should have been registered"

    echo "$output" | grep "retention" ||
        error "The retention backend should have been registered"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_lustre_source)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
