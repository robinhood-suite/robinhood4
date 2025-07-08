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

    local output="$(rbh_find --verbose "rbh:$db:$testdb" -printf "$printf")"

    echo "$output" | grep "\$project" | grep "$project"
}

test_printf()
{
    touch file

    rbh_sync "rbh:retention:." "rbh:$db:$testdb"

    check_printf_project "%e\n" '"xattrs.user.expires" : true'
    check_printf_project "%E\n" '"xattrs.trusted.expiration_date" : true'
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
