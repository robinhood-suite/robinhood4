#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/../../../rbh-sync/tests/acl/acl_utils.bash

setup_acl_files()
{
    touch owner named_user owning_group group_masked \
        named_group other masked named_user_denied \
        group_denied denied owner_denied named_group_denied

    chmod 000 ./*
}


################################################################################
#                                    TESTS                                     #
################################################################################

test_acl_rwxable()
{
    local predicate=$1
    local mode_perm=$2
    local acl_perm=$3
    local mask_perm=$4
    local expect_root=$5
    local uid
    local gid
    local group_gid
    local expected=(
        "/named_group"
        "/named_user"
        "/other"
        "/owner"
        "/owning_group"
    )

    uid=$(id -u "$test_user")
    gid=$(id -g "$test_user")
    group_gid=$(getent group "$test_group" | cut -d: -f3)

    setup_acl_files

    chown "$uid:$group_gid" owner
    chmod "u+$mode_perm" owner

    setfacl -m "u:$uid:$acl_perm" named_user

    chgrp "$gid" owning_group
    chmod "g+$mode_perm" owning_group

    setfacl -m "g:$group_gid:$acl_perm" named_group

    chmod "o+$mode_perm" other

    setfacl -m "u:$uid:$acl_perm,m::$mask_perm" masked

    chmod "o+$mode_perm" named_user_denied
    setfacl -m "u:$uid:---" named_user_denied

    chgrp "$gid" group_denied
    chmod "o+$mode_perm" group_denied

    chown "$uid:$group_gid" owner_denied
    chmod "o+$mode_perm" owner_denied

    chmod "o+$mode_perm" named_group_denied
    setfacl -m "g:$group_gid:---" named_group_denied

    chgrp "$gid" group_masked
    setfacl -m "g::$acl_perm,m::---" group_masked

    rbh_sync_acl "." "rbh:$db:$testdb"

    if [[ "$expect_root" == true ]]; then
        expected=("/" "${expected[@]}")
    fi

    rbh_find "rbh:$db:$testdb" \
        "-$predicate" "$uid:$gid,$group_gid" | sort |
        difflines "${expected[@]}"
}

test_readable()
{
    test_acl_rwxable readable r r-- --- true
}

test_writable()
{
    test_acl_rwxable writable w -w- r-x false
}

test_executable()
{
    test_acl_rwxable executable x --x rw- true
}

test_username()
{
    local group_gid

    group_gid=$(getent group "$test_group" | cut -d: -f3)

    setup_acl_files

    setfacl -m "g:$group_gid:r--" named_group

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -readable "$test_user" | sort |
        difflines \
        "/" \
        "/named_group"
}


################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_readable test_writable test_executable test_username)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
test_group="${test_user}_group"
groupadd "$test_group"
useradd -K MAIL_DIR=/dev/null -lMN "$test_user" -G "$test_group"

trap -- "rm -rf '$tmpdir'; userdel '$test_user'; groupdel '$test_group'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
