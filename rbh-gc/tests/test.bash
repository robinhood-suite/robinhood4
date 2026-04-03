#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash


test_basic()
{
    touch fileA fileB

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileA" "/fileB"

    rbh_gc "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileA" "/fileB"

    rm fileA

    rbh_gc "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/fileB"
}

test_dry_run()
{
    touch fileA fileB

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_gc -d "rbh:$db:$testdb" | difflines "0 element total to delete"

    rm fileA

    rbh_gc -d "rbh:$db:$testdb" |
        difflines "'/fileA' needs to be deleted" "1 element total to delete"
}

test_sync_gc_run()
{
    mongo_only_test

    touch fileA

    before=$(date +%s)

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    after=$(date +%s)

    rm fileA

    rbh_gc -d -s $(($before - 1)) "rbh:$db:$testdb" |
        difflines "0 element total to delete"

    rbh_gc -d -s $(($after + 1)) "rbh:$db:$testdb" |
        difflines "'/fileA' needs to be deleted" "1 element total to delete"
}

test_config()
{
    local conf_file="conf"
    local file="test_file"

    mongo_only_test

    touch $file

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf_file

    rbh_sync --config $conf_file rbh:posix:. rbh:$db:$testdb
    find_attribute '"ns.xattrs.path":"/'$file'"'

    rm $file

    echo "---
 mongo:
     address: \"mongodb://localhost:12345\"
---" > $conf_file

    rbh_gc --config $conf_file rbh:$db:$testdb &&
        error "Sync with invalid server address in config should have failed"

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf_file

    rbh_gc --config $conf_file -d "rbh:$db:$testdb" |
        difflines "'/test_file' needs to be deleted" "1 element total to delete"
}

declare -a tests=(test_basic test_dry_run test_sync_gc_run test_config)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
