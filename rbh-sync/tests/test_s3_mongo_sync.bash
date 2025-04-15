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
    # Generate credentials
    certgen -host "127.0.0.1"
    if [ ! -d ~/minio/certs ]; then
        mkdir -p ~/minio/certs
    fi

    mv -f private.key ~/minio/certs/private.key
    mv -f public.crt ~/minio/certs/public.crt
    unset http_proxy
    unset https_proxy

    # Start the MinIO server and wait for it to start to set it up
    /usr/local/bin/minio server ~/minio --console-address :9001 \
        --certs-dir ~/minio/certs >/dev/null 2>&1 &

    until ss -tuln | grep 9001 > /dev/null 2>&1; do
        echo "Waiting for MinIO to start..."
        sleep 1
    done

    mc alias set local https://127.0.0.1:9000 minioadmin minioadmin --insecure
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync()
{
    mc mb local/test-bucket --insecure
    touch test_obj
    mc cp test_obj local/test-bucket --insecure
    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"

    find_attribute '"ns.name":"test_obj"'
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"'

    mc rb --force local/test-bucket --insecure
}

test_sync_size()
{
    truncate -s 1025 "test_obj"
    mc mb local/test-bucket --insecure
    mc cp test_obj local/test-bucket --insecure
    local length=$(stat -c %s "test_obj")

    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"test-bucket/test_obj"' \
                   '"statx.size" : '$length

    mc rb --force local/test-bucket --insecure
}

test_sync_mtime()
{
    mc mb local/test-bucket --insecure
    touch test_obj
    mc cp test_obj local/test-bucket --insecure

    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"

    filemtime=$(mc stat local/test-bucket/test_obj --insecure | grep "Date" | \
                awk -F" : " '{print $2}'  | xargs -I {} date -d "{}" +%s)

    find_attribute '"statx.mtime.sec":NumberLong('$filemtime')' \
                   '"ns.name":"test_obj"'

    mc rb --force local/test-bucket --insecure
}

test_sync_empty()
{
    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"
    local count=$(count_documents)

    if [[ $count -ne 0 ]]; then
        error "Found '$count' entries instead of 0"
    fi
}

test_sync_empty_bucket()
{
    mc mb local/test-bucket --insecure

    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"
    local count=$(count_documents)

    if [[ $count -ne 0 ]]; then
        error "Found '$count' entries instead of 0"
    fi

    mc rb --force local/test-bucket --insecure
}

test_sync_custom_metadata()
{
    mc mb local/test-bucket --insecure
    touch test_obj
    mc cp --attr "test_metadata=test_value" test_obj \
                 local/test-bucket/test_obj --insecure
    rm test_obj

    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"
    find_attribute '"ns.name":"test_obj"'
    find_attribute '"xattrs.user_metadata.test_metadata":"test_value"'

    mc rb --force local/test-bucket --insecure
}

test_sync_multi_buckets()
{
    touch test_obj

    for i in `seq 1 5`
    do
        mc mb local/test-bucket-$i --insecure
        mc cp test_obj local/test-bucket-$i/test_obj_$i --insecure
    done

    rbh_sync "rbh:s3:." "rbh:mongo:$testdb"

    for i in `seq 1 5`
    do
        find_attribute '"ns.name":"test_obj_'$i'"'
        mc rb --force local/test-bucket-$i --insecure
    done
}

################################################################################
#                                     MAIN                                     #
################################################################################

minio_teardown()
{
    mc rb --force --dangerous local --insecure
    mc admin service stop local --insecure
    mc alias remove local
}

declare -a tests=(test_sync test_sync_size test_sync_mtime test_sync_empty
                  test_sync_empty_bucket test_sync_custom_metadata
                  test_sync_multi_buckets)
trap -- "minio_teardown" EXIT

minio_setup
run_tests ${tests[@]}
minio_teardown
