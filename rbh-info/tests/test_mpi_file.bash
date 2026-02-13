#!/usr/bin/env bash

# This file is part of rbh-info.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_mpi_file_list()
{
    local output=$(rbh_info --list)

    echo "$output" | grep -q "mpi-file"
    echo "$output" | grep -q "posix"
    echo "$output" | grep -q "mfu"
}

test_mpi_file_source()
{
    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:$db:$testdb"

    local output=$(rbh_info "rbh:$db:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 1)); then
        error "One backend should have been registered"
    fi

    echo "$output" | grep "mpi-file" ||
        error "The mpi-file backend should have been registered"
}

test_command_backend()
{
    dwalk -q -o "$testdb.mfu" .
    rbh_sync "rbh:mpi-file:$testdb.mfu" "rbh:$db:$testdb"

    local command_backend=$(rbh_info "rbh:$db:$testdb" -B)

    if [ "$command_backend" != "mpi-file" ]; then
        error "Command backends don't match, found '$command_backend', expected 'mpi-file'\n"
    fi

    command_backend=$(rbh_info "rbh:$db:$testdb" --command-backend)

    if [ "$command_backend" != "mpi-file" ]; then
        error "Command backends don't match, found '$command_backend', expected 'mpi-file'\n"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_mpi_file_list test_mpi_file_source test_command_backend)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
