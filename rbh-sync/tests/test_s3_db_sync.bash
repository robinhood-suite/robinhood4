#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

rbh_sync_s3()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync "rbh:s3-mpi:$1" "$2"
    else
        rbh_sync "rbh:s3:$1" "$2"
    fi
}

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
    rbh_sync_s3 "" "rbh:$db:$testdb"

    find_attribute '"ns.name":"test_obj"'
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"'
}

test_sync_size()
{
    truncate -s 1025 "test_obj"
    mc mb local/test-bucket
    mc cp test_obj local/test-bucket
    local length=$(stat -c %s "test_obj")

    rbh_sync_s3 "" "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"' \
                   '"statx.size" : '$length
}

test_sync_mtime()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp test_obj local/test-bucket

    rbh_sync_s3 "" "rbh:$db:$testdb"

    local filemtime=$(mc stat local/test-bucket/test_obj | grep "Date" | \
                      awk -F" : " '{print $2}'  | xargs -I {} date -d "{}" +%s)

    find_attribute '"statx.mtime.sec":NumberLong('$filemtime')' \
                   '"ns.name":"test_obj"'
}

test_sync_empty()
{
    rbh_sync_s3 "" "rbh:$db:$testdb"
    local count=$(count_documents)

    if [[ $count -ne 0 ]]; then
        error "Found '$count' entries instead of 0"
    fi
}

test_sync_empty_bucket()
{
    mc mb local/test-bucket

    rbh_sync_s3 "" "rbh:$db:$testdb"
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

    rbh_sync_s3 "" "rbh:$db:$testdb"
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

    rbh_sync_s3 "" "rbh:$db:$testdb"

    for i in `seq 1 5`
    do
        find_attribute '"ns.name":"test_obj_'$i'"'
    done
}

test_sync_mixed_buckets()
{
    touch test_obj

    for i in `seq 1 5`
    do
        mc mb local/test-bucket-$i
    done

    for i in 2 3 4
    do
        mc cp test_obj local/test-bucket-$i/test_obj_$i
    done

    mc cp test_obj local/test-bucket-3/test_obj_3-

    rbh_sync_s3 "" "rbh:$db:$testdb"

    find_attribute '"ns.name":"test_obj_3-"'

    for i in 2 3 4
    do
        find_attribute '"ns.name":"test_obj_'$i'"'
    done
}

test_sync_branch()
{
    local bucket_1="test-bucket-1"
    local bucket_2="test-bucket-2"

    local object_1="test-1"
    local object_2="test-2"

    mc mb local/$bucket_1
    mc mb local/$bucket_2
    touch test_obj
    mc cp test_obj local/$bucket_1/$object_1
    mc cp test_obj local/$bucket_2/$object_2

    local output=$(rbh_sync_s3 "#not_a_bucket" "rbh:$db:$testdb" 2>&1)

    if [[ $output != *"specified bucket does not exist"* ]]; then
        error "Branching with invalid bucket name should have failed"
    fi

    rbh_sync_s3 "#$bucket_1" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$object_1'"'
    find_attribute '"ns.xattrs.path":"'$bucket_1/$object_1'"'
    ! (find_attribute '"ns.xattrs.path":"'$bucket_2/$object_2'"')

    rbh_sync_s3 "#$bucket_2" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$object_2'"'
    find_attribute '"ns.xattrs.path":"'$bucket_2/$object_2'"'
}

test_sync_valid_uri()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp test_obj local/test-bucket

    rbh_sync "rbh://minioadmin:minioadmin@127.0.0.1:9000/s3:" "rbh:$db:$testdb"

    find_attribute '"ns.name":"test_obj"'
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"'
}

test_sync_partial_uri()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp test_obj local/test-bucket

    rbh_sync "rbh://127.0.0.1:9000/s3:" "rbh:$db:$testdb"

    find_attribute '"ns.name":"test_obj"'
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"'
}

test_sync_invalid_uri()
{
    mc mb local/test-bucket
    touch test_obj
    mc cp test_obj local/test-bucket
    local error_message="The Access Key Id you \
                         provided does not exist in our records"

    local output=$(rbh_sync \
                   "rbh://wrong-user:wrong-password@127.0.0.1:9000/s3:"
                   "rbh:$db:$testdb" 2>&1)

    if echo "$output" | grep -q "$error_message"; then
        error "Sync with URI containing wrong informations should " \
              "have failed"
    fi
}

test_sync_with_region()
{
    local conf="test_conf"
    local error_message="Failed to connect to s3.eu-west-3.amazonaws.com"

    touch $conf

cat > $conf <<EOF
s3:
  region : "eu-west-3"
  user: minioadmin
  password: minioadmin
EOF

    local output=$(rbh_sync --config "$conf" "rbh:s3:" "rbh:$db:$testdb")

    if echo "$output" | grep -q "$error_message"; then
        error "Sync with no address and a port should try to connect to the" \
              "aws server of the region"
    fi

    rm -rf conf.yaml
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
                  test_sync_multi_buckets test_sync_mixed_buckets
                  test_sync_valid_uri test_sync_partial_uri
                  test_sync_invalid_uri test_sync_with_region)

if [[ $WITH_MPI == false ]]; then
    tests+=(test_sync_branch)
fi

trap -- "minio_teardown" EXIT


sub_teardown=minio_teardown
minio_setup
run_tests ${tests[@]}
