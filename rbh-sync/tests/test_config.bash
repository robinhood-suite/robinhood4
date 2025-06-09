#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

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
mongodb_address: \"mongodb://localhost:12345\"
mongodb_address: \"mongodb://localhost:27017\"
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

declare -a tests=(test_invalid_config test_valid_config test_env)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
