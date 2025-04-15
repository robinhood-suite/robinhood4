#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

minio_setup()
{
    export MC_INSECURE=1
    yes | mc alias set local https://127.0.0.1:9000 minioadmin minioadmin
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp test_obj local/test-bucket
    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    find_attribute '"ns.name":"test_obj"'
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"'
}

test_sync_size()
{
    truncate -s 1025 "test_obj"
    mc mb local/test-bucket
    mc cp test_obj local/test-bucket
    local length=$(stat -c %s "test_obj")

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"' \
                   '"statx.size" : '$length
}

test_sync_mtime()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp test_obj local/test-bucket

    rbh_sync "rbh:s3:." "rbh:$db:$testdb"

    local filemtime=$(mc stat local/test-bucket/test_obj | grep "Date" | \
                      awk -F" : " '{print $2}'  | xargs -I {} date -d "{}" +%s)

    find_attribute '"statx.mtime.sec":NumberLong('$filemtime')' \
                   '"ns.name":"test_obj"'
}

test_sync_empty()
{
    rbh_sync "rbh:s3:." "rbh:$db:$testdb"
    local count=$(count_documents)

    if [[ $count -ne 0 ]]; then
        error "Found '$count' entries instead of 0"
    fi
}

test_sync_empty_bucket()
{
    mc mb local/test-bucket

    rbh_sync "rbh:s3:." "rbh:$db:$testdb"
    local count=$(count_documents)

    if [[ $count -ne 0 ]]; then
        error "Found '$count' entries instead of 0"
    fi
}

test_sync_custom_metadata()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp --attr "test_metadata1=test_value1;test_metadata2=test_value2" \
                  test_obj local/test-bucket/test_obj
    rm test_obj

    rbh_sync "rbh:s3:." "rbh:$db:$testdb"
    find_attribute '"ns.name":"test_obj"'
    find_attribute '"xattrs.user_metadata.test_metadata1":"test_value1"'
    find_attribute '"xattrs.user_metadata.test_metadata2":"test_value2"'
}

test_sync_multi_buckets()
{
    touch test_obj

    for i in `seq 1 5`
    do
        mc mb local/test-bucket-$i
        mc cp test_obj local/test-bucket-$i/test_obj_$i
    done

    rbh_sync "rbh:s3:." "rbh:$db:$testdb"

    for i in `seq 1 5`
    do
        find_attribute '"ns.name":"test_obj_'$i'"'
        mc rb --force local/test-bucket-$i
    done
}

################################################################################
#                                     MAIN                                     #
################################################################################

minio_teardown()
{
    mc rb --force --dangerous local
}

declare -a tests=(test_sync test_sync_size test_sync_mtime test_sync_empty
                  test_sync_empty_bucket test_sync_custom_metadata
                  test_sync_multi_buckets)
trap -- "minio_teardown" EXIT


sub_teardown=minio_teardown
minio_setup
run_tests ${tests[@]}
