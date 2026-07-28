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
    mkdir acl
    cd acl

    touch owner named_user owning_group group_masked \
        named_group other masked named_user_denied \
        group_denied denied owner_denied named_group_denied

    chmod 000 ./*
}


################################################################################
#                                    TESTS                                     #
################################################################################

test_readable()
{
    local uid
    local gid
    local group_gid

    uid=$(id -u "$test_user")
    gid=$(id -g "$test_user")
    group_gid=$(getent group "$test_group" | cut -d: -f3)

    setup_acl_files

    chown "$uid:$group_gid" owner
    chmod u+r owner

    setfacl -m "u:$uid:r--" named_user

    chgrp "$gid" owning_group
    chmod g+r owning_group

    setfacl -m "g:$group_gid:r--" named_group

    chmod o+r other

    setfacl -m "u:$uid:r--,m::---" masked

    chmod o+r named_user_denied
    setfacl -m "u:$uid:---" named_user_denied

    chgrp "$gid" group_denied
    chmod o+r group_denied

    chown "$uid:$group_gid" owner_denied
    chmod o+r owner_denied

    chmod o+r named_group_denied
    setfacl -m "g:$group_gid:---" named_group_denied

    chgrp "$gid" group_masked
    setfacl -m "g::r--,m::---" group_masked

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-readable "$uid:$gid,$group_gid" | sort |
        difflines \
        "/" \
        "/named_group" \
        "/named_user" \
        "/other" \
        "/owner" \
        "/owning_group"
}

test_writable()
{
    local uid
    local gid
    local group_gid

    uid=$(id -u "$test_user")
    gid=$(id -g "$test_user")
    group_gid=$(getent group "$test_group" | cut -d: -f3)

    setup_acl_files

    chown "$uid:$group_gid" owner
    chmod u+w owner

    setfacl -m "u:$uid:-w-" named_user

    chgrp "$gid" owning_group
    chmod g+w owning_group

    setfacl -m "g:$group_gid:-w-" named_group

    chmod o+w other

    setfacl -m "u:$uid:-w-,m::r-x" masked

    chmod o+w named_user_denied
    setfacl -m "u:$uid:r--" named_user_denied

    chgrp "$gid" group_denied
    chmod o+w group_denied

    chown "$uid:$group_gid" owner_denied
    chmod o+w owner_denied

    chmod o+w named_group_denied
    setfacl -m "g:$group_gid:---" named_group_denied

    chgrp "$gid" group_masked
    setfacl -m "g::-w-,m::---" group_masked

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-writable "$uid:$gid,$group_gid" | sort |
        difflines \
        "/named_group" \
        "/named_user" \
        "/other" \
        "/owner" \
        "/owning_group"
}

test_executable()
{
    local uid
    local gid
    local group_gid

    uid=$(id -u "$test_user")
    gid=$(id -g "$test_user")
    group_gid=$(getent group "$test_group" | cut -d: -f3)

    setup_acl_files

    chown "$uid:$group_gid" owner
    chmod u+x owner

    setfacl -m "u:$uid:--x" named_user

    chgrp "$gid" owning_group
    chmod g+x owning_group

    setfacl -m "g:$group_gid:--x" named_group

    chmod o+x other

    setfacl -m "u:$uid:--x,m::rw-" masked

    chmod o+x named_user_denied
    setfacl -m "u:$uid:r--" named_user_denied

    chgrp "$gid" group_denied
    chmod o+x group_denied

    chown "$uid:$group_gid" owner_denied
    chmod o+x owner_denied

    chmod o+r named_group_denied
    setfacl -m "g:$group_gid:---" named_group_denied

    chgrp "$gid" group_masked
    setfacl -m "g::--x,m::---" group_masked

    rbh_sync_acl "." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" \
        -acl-executable "$uid:$gid,$group_gid" | sort |
        difflines \
        "/" \
        "/named_group" \
        "/named_user" \
        "/other" \
        "/owner" \
        "/owning_group"
}


################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_readable test_writable test_executable)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
test_group="${test_user}_group"
groupadd "$test_group"
useradd -K MAIL_DIR=/dev/null -lMN "$test_user" -G "$test_group"

trap -- "rm -rf '$tmpdir'; userdel '$test_user'; groupdel '$test_group'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
