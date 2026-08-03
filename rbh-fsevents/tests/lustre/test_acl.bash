#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

invoke_rbh-fsevents_acl()
{
    rbh_fsevents --enrich rbh:lustre-acl:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_acl()
{
    local dir="test_dir"
    local entry="test_file"
    local acl_uid=123456
    local acl_gid=3456

    mkdir "$dir"

    # The file must inherit this default ACL when it is created.
    setfacl -d -m "u:$acl_uid:r--,g:$acl_gid:--x" "$dir"

    touch "$dir/$entry"

    invoke_rbh-fsevents_acl

    find_attribute \
        '"xattrs.acl.access.users":{$elemMatch:{id:'"$acl_uid"',p:4}}' \
        '"xattrs.acl.access.groups":{$elemMatch:{id:'"$acl_gid"',p:1}}' \
        '"ns.name":"'$entry'"'

    setfacl -m "u:$acl_uid:r-x,g:$acl_gid:r--" "$dir/$entry"

    invoke_rbh-fsevents_acl

    find_attribute \
        '"xattrs.acl.access.users":{$elemMatch:{id:'"$acl_uid"',p:5}}' \
        '"xattrs.acl.access.groups":{$elemMatch:{id:'"$acl_gid"',p:4}}' \
        '"ns.name":"'$entry'"'
}


################################################################################
#                                     MAIN                                     #
################################################################################

# Since Lustre changelog handling of POSIX ACL updates is not fixed yet,
# this test cannot pass.
# TODO: Enable this test once Lustre reports ACL changes correctly.
declare -a tests=(
#    test_acl
)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
