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

test_posix_source()
{
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 1)); then
        error "One backend should have been registered"
    fi

    echo "$output" | grep "posix" ||
        error "Only the POSIX backend should have been registered"
}

test_retention_source()
{
    rbh_sync "rbh:retention:." "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 2)); then
        error "Two backends should have been registered"
    fi

    echo "$output" | grep "posix" ||
        error "The POSIX backend should have been registered"

    echo "$output" | grep "retention" ||
        error "The retention backend should have been registered"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_posix_source test_retention_source)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
