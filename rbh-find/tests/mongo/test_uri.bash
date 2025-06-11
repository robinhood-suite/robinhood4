#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid_uri()
{
    mongo $testdb --eval 'db.dropUser("test")'

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh://test/mongo:$testdb" &&
        error "Should have failed because of invalid host"

    rbh_find "rbh://localhost:12345/mongo:$testdb" &&
        error "Should have failed because of invalid port"

    rbh_find "rbh://user@localhost:27017/mongo:$testdb" &&
        error "Should have failed because of unregistered user"

    mongo $testdb --eval 'db.createUser({"user": "test",
                                         "pwd": "1234",
                                         roles: [ "readWrite", "dbAdmin" ]})'

    rbh_find "rbh://test:something/mongo:$testdb" &&
        error "Should have failed because of invalid password"

    mongo $testdb --eval 'db.dropUser("test")'

    return 0
}

test_uri()
{
    mkdir db_repo
    mongod --fork --logpath log_file --dbpath db_repo --port 27018
    mongo --port 27018 $testdb --eval 'db.createUser({"user": "test",
                                                      "pwd": "42",
                                                      roles: [ "readWrite",
                                                               "dbAdmin" ]})'

    rbh_sync "rbh:posix:log_file" "rbh://test:42@localhost:27018/mongo:$testdb"

    rbh_find "rbh://test:42@localhost:27018/mongo:$testdb" | difflines "/"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid_uri test_uri)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

sub_teardown=mongo_teardown
run_tests ${tests[@]}
