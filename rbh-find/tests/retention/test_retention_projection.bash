#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

check_printf_project()
{
    local printf=$1
    local project=$2
    local conf=$3

    local output="$(rbh_find --config $conf --verbose "rbh:$db:$testdb" \
                    -printf "$printf")"

    echo "$output" | grep "\$project" | grep "$project"
}

test_printf()
{
    local conf_file="conf_file"

    touch file

    rbh_sync "rbh:retention:." "rbh:$db:$testdb"

    check_printf_project "%e\n" '"xattrs.user.expires" : true'
    check_printf_project "%E\n" '"xattrs.trusted.expiration_date" : true'

    do_db drop $testdb

    echo "---
RBH_RETENTION_XATTR: \"user.blob\"
backends:
    retention:
        extends: posix
        enrichers:
            - retention
---" > $conf_file

    rbh_sync --config $conf_file "rbh:retention:." "rbh:$db:$testdb"

    check_printf_project "%e\n" '"xattrs.user.blob" : true' $conf_file
    check_printf_project "%E\n" '"xattrs.trusted.expiration_date" : true' \
        $conf_file
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_printf)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user || true" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
