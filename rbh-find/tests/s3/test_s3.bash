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

test_name()
{
    mc mb local/bucket
    touch obj1
    touch obj2

    mc cp obj1 local/bucket
    mc cp obj2 local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -name "obj1" | sort |
        difflines "bucket/obj1"
    rbh_find "rbh:$db:$testdb" -name "obj*" | sort |
        difflines "bucket/obj1" "bucket/obj2"
    rbh_find "rbh:$db:$testdb" -name "object" | sort | difflines

    rm -f obj1 obj2
}

test_size()
{
    mc mb local/bucket
    touch empty
    truncate --size 1K 1K
    truncate --size 1025 1K+1

    mc cp empty local/bucket
    mc cp 1K local/bucket
    mc cp 1K+1 local/bucket

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -size 1k | sort | difflines "bucket/1K"
    rbh_find "rbh:$db:$testdb" -size +1k | sort | difflines "bucket/1K+1"
    rbh_find "rbh:$db:$testdb" -size -1k | sort | difflines "bucket/empty"

    rm -f empty 1K 1K+1
}

test_bucket()
{
    mc mb local/bucket1
    mc mb local/bucket2

    touch fileA
    touch fileB

    mc cp fileA local/bucket1
    mc cp fileB local/bucket2

    rbh_sync "rbh:s3:" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -bucket bucket1 | sort |
        difflines "bucket1/fileA"
    rbh_find "rbh:$db:$testdb" -bucket bucket | sort | difflines
    rbh_find "rbh:$db:$testdb" -bucket bucket* | sort |
        difflines "bucket1/fileA" "bucket2/fileB"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_name test_size test_bucket)

trap -- "minio_teardown" EXIT


sub_teardown=minio_teardown
minio_setup
run_tests ${tests[@]}
