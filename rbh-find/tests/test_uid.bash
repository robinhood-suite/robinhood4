#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_equal()
{
    touch "my_file"
    touch "your_file"

    useradd -MU you
    my_id=$(id -u)
    your_id=$(id -u you)
    chown you "your_file"
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -uid $my_id | sort |
        difflines "/" "/my_file"
    rbh_find "rbh:mongo:$testdb" -uid $your_id | sort |
        difflines "/your_file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_equal)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
