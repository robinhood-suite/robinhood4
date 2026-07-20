#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/acl_utils.bash


################################################################################
#                                    TESTS                                     #
################################################################################

test_access_acl()
{
    touch file

    set_access_acl \
        "u::rw-,u:424242:rw-,g::r--,g:434343:r-x,m::rwx,o::---" \
        file

    rbh_sync_acl "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
               '"xattrs.acl.access.group":4' \
               '"xattrs.acl.access.users": { $size: 1 }' \
               '"xattrs.acl.access.users.0.id":424242' \
               '"xattrs.acl.access.users.0.p":6' \
               '"xattrs.acl.access.groups": { $size: 1 }' \
               '"xattrs.acl.access.groups.0.id":434343' \
               '"xattrs.acl.access.groups.0.p":5'
}

test_default_acl()
{
    mkdir dir

    set_default_acl \
        "u::rw-,u:424242:rw-,g::r--,g:434343:r-x,m::rwx,o::---" \
        dir

    rbh_sync_acl "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/dir"' \
               '"xattrs.acl.default.owner":6' \
               '"xattrs.acl.default.group":4' \
               '"xattrs.acl.default.mask":7' \
               '"xattrs.acl.default.users": { $size: 1 }' \
               '"xattrs.acl.default.users.0.id":424242' \
               '"xattrs.acl.default.users.0.p":6' \
               '"xattrs.acl.default.groups": { $size: 1 }' \
               '"xattrs.acl.default.groups.0.id":434343' \
               '"xattrs.acl.default.groups.0.p":5'
}

test_access_and_default_acl()
{
    mkdir dir

    set_access_acl \
        "u::rw-,u:424242:r--,g::r-x,g:434343:-wx,m::rwx,o::---" \
        dir

    set_default_acl \
        "u::rw-,u:424243:r-x,g::r--,g:434344:rw-,m::rwx,o::--x" \
        dir

    rbh_sync_acl "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/dir"' \
               '"xattrs.acl.access.group":5' \
               '"xattrs.acl.access.users.0.id":424242' \
               '"xattrs.acl.access.users.0.p":4' \
               '"xattrs.acl.access.groups.0.id":434343' \
               '"xattrs.acl.access.groups.0.p":3' \
               '"xattrs.acl.default.owner":6' \
               '"xattrs.acl.default.group":4' \
               '"xattrs.acl.default.mask":7' \
               '"xattrs.acl.default.users.0.id":424243' \
               '"xattrs.acl.default.users.0.p":5' \
               '"xattrs.acl.default.groups.0.id":434344' \
               '"xattrs.acl.default.groups.0.p":6'
}

test_no_acl()
{
    touch file

    rbh_sync_acl "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
               '"xattrs.acl":{ $exists: false }'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_access_acl
    test_default_acl
    test_access_and_default_acl
    test_no_acl
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
