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

test_s3_source()
{
    rbh_sync "rbh:s3:" "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 7)); then
        error "One backend and 5 connection parameters should have been printed"
    fi

    echo "$output" | grep "s3" ||
        error "Only the S3 backend should have been registered"

    echo "$output" | grep "address" ||
        error "The address used should have been registered"

    echo "$output" | grep "region" ||
        error "The region used should have been registered"

    echo "$output" | grep "crt_path" ||
        error "The crt_path used should have been registered"

    echo "$output" | grep "user" ||
        error "The user used should have been registered"

    echo "$output" | grep "password" ||
        error "The password used should have been registered"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_s3_source)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
