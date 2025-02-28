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

test_uid_user_equal()
{
    touch "my_file"
    touch "your_file"

    local test_user="$(get_test_user $FUNCNAME)"
    add_test_user $test_user
    me=$(id -un)
    my_id=$(id -u)
    your_id=$(id -u $test_user)
    chown $test_user "your_file"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -uid $my_id | sort | difflines "/" "/my_file"
    rbh_find "rbh:mongo:$testdb" -uid $your_id | sort | difflines "/your_file"

    rbh_find "rbh:mongo:$testdb" -user $me | sort | difflines "/" "/my_file"
    rbh_find "rbh:mongo:$testdb" -user $test_user | sort |
        difflines "/your_file"

    rbh_find "rbh:mongo:$testdb" -nouser | sort | difflines

    userdel $test_user

    rbh_find "rbh:mongo:$testdb" -nouser | sort | difflines "/your_file"
}

test_gid_group_equal()
{
    touch "my_file"
    touch "your_file"

    groupadd grptest || true
    my_grp=$(id -gn)
    my_gid=$(id -g)
    test_gid=$(getent group grptest | cut -d: -f3)
    chgrp grptest "your_file"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -gid $my_gid | sort | difflines "/" "/my_file"
    rbh_find "rbh:mongo:$testdb" -gid $test_gid | sort | difflines "/your_file"

    rbh_find "rbh:mongo:$testdb" -group $my_grp | sort |
        difflines "/" "/my_file"
    rbh_find "rbh:mongo:$testdb" -group grptest | sort | difflines "/your_file"

    rbh_find "rbh:mongo:$testdb" -nogroup | sort | difflines

    groupdel grptest

    rbh_find "rbh:mongo:$testdb" -nogroup | sort | difflines "/your_file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_gid_group_equal test_uid_user_equal)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'; userdel test || true; groupdel grptest || true" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
