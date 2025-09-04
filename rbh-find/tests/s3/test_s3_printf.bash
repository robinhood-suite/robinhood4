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

test_filename()
{
    mc mb local/bucket
    touch obj
    mc cp obj local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -name obj -printf "%f\n" | sort |
        difflines "obj"
}

test_backend_name()
{
    mc mb local/bucket
    rbh_sync "rbh:s3:" "rbh:$db:other"
    touch obj
    mc cp obj local/bucket
    rbh_sync "rbh:s3:" "rbh:$db:$testdb"
    rbh_sync "rbh:s3:" "rbh:$db:${testdb}2"

    rbh_find "rbh:$db:$testdb" "rbh:$db:other" "rbh:$db:${testdb}2" \
        -name obj -printf "%H\n" | sort |
            difflines "rbh:$db:$testdb" "rbh:$db:${testdb}2"

    do_db drop "other"
    do_db drop "${testdb}2"
}

test_size()
{
    mc mb local/bucket
    truncate -s 512 obj
    mc cp obj local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    local s=$(rbh_find "rbh:$db:$testdb" -name obj -printf "%s\n")
    local size=$(stat -c %s obj)

    if [[ $s != $size ]]; then
        error "wrong size: $s != $size"
    fi
}

test_id()
{
    mc mb local/bucket
    touch objA
    touch objB
    mc cp objA local/bucket
    mc cp objB local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    local IDA=$(rbh_find "rbh:$db:$testdb" -name objA -printf "%I\n")
    local IDB=$(rbh_find "rbh:$db:$testdb" -name objB -printf "%I\n")

    local countA=$(do_db count "$testdb" '"_id": BinData(0, "'$IDA'")')
    local countB=$(do_db count "$testdb" '"_id": BinData(0, "'$IDB'")')

    if [[ "$countA" != "1" ]]; then
        error "Couldn't find entry with ID '$IDA'"
    fi

    if [[ "$countB" != "1" ]]; then
        error "Couldn't find entry with ID '$IDB'"
    fi
}

test_path()
{
    mc mb local/bucket
    touch obj
    mc cp obj local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p\n" -name obj | sort |
        difflines "bucket/obj"
}

test_bucket()
{
    mc mb local/bucket1
    mc mb local/bucket2
    touch obj1
    touch obj2
    mc cp obj1 local/bucket1
    mc cp obj2 local/bucket2

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%b\n" | sort |
        difflines "bucket1" "bucket2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_filename test_backend_name test_size test_id test_path
                  test_bucket)

trap -- "minio_teardown" EXIT


sub_teardown=minio_teardown
minio_setup
run_tests ${tests[@]}
