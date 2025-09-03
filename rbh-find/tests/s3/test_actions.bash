#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

minio_setup()
{
    export MC_INSECURE=1
    yes | mc alias set local https://127.0.0.1:9000 minioadmin minioadmin
}

minio_teardown()
{
    mc rb --force --dangerous local
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_delete()
{
    mc mb local/bucket
    touch to_delete
    touch do_not_delete

    mc cp to_delete local/bucket
    mc cp do_not_delete local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"
    rbh_find "rbh:$db:$testdb" -name to_delete -delete

    local output=$(mc find local/bucket --name do_not_delete | wc -l)
    if [[ $output -ne 1 ]]; then
        error "Object 'do_not_delete' should not have been deleted"
    fi

    output=$(mc find local/bucket --name to_delete | wc -l)
    if [[ $output -ne 0 ]]; then
        error "Object 'to_delete' should have been deleted"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_delete)

trap -- "minio_teardown" EXIT


sub_teardown=minio_teardown
minio_setup
run_tests ${tests[@]}
