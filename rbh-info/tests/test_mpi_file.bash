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

test_mpi_file_source()
{
    touch fileA
    mkdir dir
    touch dir/file1

    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 1)); then
        error "One backend should have been registered"
    fi

    echo "$output" | grep "mpi-file" ||
        error "The mpi-file backend should have been registered"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_mpi_file_source)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
