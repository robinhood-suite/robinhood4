#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_readable()
{
    # Only the root can read it
    touch "file"
    chmod o-r "file"

    # Only second_user can read it
    touch "second_user_file"
    chmod o-r "second_user_file"
    chmod g-r "second_user_file"
    chown $second_user "second_user_file"

    # Only test_user can read it
    touch "user_file"
    chmod o-r "user_file"
    chmod g-r "user_file"
    chown $test_user "user_file"

    # Only the group can read it
    touch "group_file"
    chmod u-r "group_file"
    chmod o-r "group_file"
    local grp=$(id -ng $test_user)
    chgrp $grp "group_file"

    # Only other can read it
    touch "other_file"
    chmod u-r "other_file"
    chmod g-r "other_file"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -readable $test_user | sort |
        difflines "/" "/group_file" "/other_file" "/user_file"

    rbh_find "rbh:$db:$testdb" -readable $second_user | sort |
        difflines "/" "/other_file" "/second_user_file"
}

test_writeable()
{
    # Only the root can write on it
    touch "file"
    chmod u+w "file"
    chmod o-w "file"

    # Only second_user can write on it
    touch "second_user_file"
    chmod u+w "second_user_file"
    chmod o-w "second_user_file"
    chmod g-w "second_user_file"
    chown $second_user "second_user_file"

    # Only test_user can write on it
    touch "user_file"
    chmod u+w "user_file"
    chmod o-w "user_file"
    chmod g-w "user_file"
    chown $test_user "user_file"

    # Only the group can write on it
    touch "group_file"
    chmod g+w "group_file"
    chmod u-w "group_file"
    chmod o-w "group_file"
    local grp=$(id -ng $test_user)
    chgrp $grp "group_file"

    # Only other can write on it
    touch "other_file"
    chmod o+w "other_file"
    chmod u-w "other_file"
    chmod g-w "other_file"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -writeable $test_user | sort |
        difflines "/group_file" "/other_file" "/user_file"

    rbh_find "rbh:$db:$testdb" -writeable $second_user | sort |
        difflines "/other_file" "/second_user_file"
}

test_executable()
{
    # Only the root can launch it
    touch "file"
    chmod u+x "file"
    chmod o-x "file"

    # Only second_user can launch it
    touch "second_user_file"
    chmod u+x "second_user_file"
    chmod o-x "second_user_file"
    chmod g-x "second_user_file"
    chown $second_user "second_user_file"

    # Only test_user can launch it
    touch "user_file"
    chmod u+x "user_file"
    chmod o-x "user_file"
    chmod g-x "user_file"
    chown $test_user "user_file"

    # Only the group can launch it
    touch "group_file"
    chmod g+x "group_file"
    chmod u-x "group_file"
    chmod o-x "group_file"
    local grp=$(id -ng $test_user)
    chgrp $grp "group_file"

    # Only other can launch it
    touch "other_file"
    chmod o+x "other_file"
    chmod u-x "other_file"
    chmod g-x "other_file"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -executable $test_user | sort |
        difflines "/" "/group_file" "/other_file" "/user_file"

    rbh_find "rbh:$db:$testdb" -executable $second_user | sort |
        difflines "/" "/other_file" "/second_user_file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_readable test_writeable test_executable)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
second_user="$(echo "$test_user"_bis)"
groupadd "second_group"
useradd -K MAIL_DIR=/dev/null -lMN $second_user -g "second_group"

trap -- "rm -rf '$tmpdir'; delete_test_user $test_user;
         userdel $second_user; groupdel second_group" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
