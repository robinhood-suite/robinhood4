#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid_config()
{
    local conf_file="conf"
    local file="test_file"

    touch $file

    echo "abcdef
ghijkl
blob" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb &&
        error "Sync with invalid configuration file should have failed"

    echo "---
backends:
    test:
        extends: posix
backends:
    test:
        extends: posix
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:test:$file rbh:$db:$testdb &&
        error "Sync with duplicate keys should have failed"

    return 0
}

test_valid_config()
{
    local conf_file="conf"
    local file="test_file"

    echo "---
 blob: \"blobby\"
 something: \"nothing\"
---" > $conf_file

    touch $file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb

    find_attribute '"ns.xattrs.path":"/'$file'"'
}

test_complete_config()
{
    local conf_file="conf"
    local file="test_file"

    mongo_only_test

    touch $file

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb

    find_attribute '"ns.xattrs.path":"/'$file'"'

    echo "---
 mongo:
     address: \"mongodb://localhost:12345\"
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb &&
        error "Sync with invalid server address in config should have failed"

    echo "---
 mongo:
     address: !int32 12345
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb &&
        error "Sync with invalid typing for db address in config should have" \
              "failed"

    echo "---
xattrs_map:
    user.blob_int32: int32
    user.blob_int64: int64
    user.blob_uint32: unsigned int32
    user.blob_uint64: unsigned int64
    user.blob_string: string
    user.blob_boolean: boolean
---" > $conf_file

    setfattr -n user.blob_int32 -v 1 $file
    setfattr -n user.blob_int64 -v 2 $file
    setfattr -n user.blob_uint32 -v 3 $file
    setfattr -n user.blob_uint64 -v 4 $file
    setfattr -n user.blob_string -v five $file
    setfattr -n user.blob_boolean -v true $file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb

    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_int32" : 1'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_int64" : 2'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_uint32" : 3'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_uint64" : 4'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_string" : "five"'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_boolean" : true'
}

test_env()
{
    local conf_file="conf"
    local file="test_file"

    cat <<- EOF > "$conf_file"
abcdef
ghijkl
blob
EOF

    export RBH_CONFIG_PATH="$conf_file"

    rbh_sync --one rbh:posix:$file rbh:$db:$testdb &&
        error "Sync with invalid configuration file should have failed"

    return 0
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid_config test_valid_config test_complete_config
                  test_env)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
