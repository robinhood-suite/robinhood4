#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid_config()
{
    local conf_file="conf"
    local file="test_file"

    echo "abcdef
ghijkl
blob" > $conf_file

    touch $file

    rbh_sync --conf $conf_file --one rbh:posix:$file rbh:mongo:$testdb &&
        error "Sync with invalid configuration file should have failed"

    return 0
}

test_valid_config()
{
    local conf_file="conf"
    local file="test_file"

    echo "---
 RBH_MONGO_DB_URI: \"mongodb://localhost:27017\"
---" > $conf_file

    touch $file

    rbh_sync --conf $conf_file --one rbh:posix:$file rbh:mongo:$testdb

    find_attribute '"ns.xattrs.path":"/"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid_config test_valid_config)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
