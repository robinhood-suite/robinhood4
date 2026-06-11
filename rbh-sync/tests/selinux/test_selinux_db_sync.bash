#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/selinux_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_selinux_context_decomposition()
{
    local ctx
    local seuser
    local serole
    local setype
    local serange

    touch file
    set_selinux_range "s0:c0" file

    ctx="$(get_selinux_context file)"
    IFS=':' read -r seuser serole setype serange <<< "$ctx"

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
               "\"xattrs.selinux.context\":\"$ctx\"" \
               "\"xattrs.selinux.user\":\"$seuser\"" \
               "\"xattrs.selinux.role\":\"$serole\"" \
               "\"xattrs.selinux.type\":\"$setype\"" \
               "\"xattrs.selinux.range\":\"$serange\""
}


test_selinux_simple_range()
{
    local count

    touch file
    set_selinux_range "s0:c0" file

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.range":"s0:c0"' \
                   '"xattrs.selinux.high_sens":0' \
                   '"xattrs.selinux.low_cat": { $size: 1 }' \
                   '"xattrs.selinux.low_cat.0.f":0' \
                   '"xattrs.selinux.low_cat.0.l":0' \
                   '"xattrs.selinux.high_cat": { $size: 1}' \
                   '"xattrs.selinux.high_cat.0.f":0' \
                   '"xattrs.selinux.high_cat.0.l":0' 
}


test_selinux_complete_range()
{
    local count

    touch file
    set_selinux_range "s0:c1,c4-s4:c1,c4,c7.c9" file

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.range":"s0:c1,c4-s4:c1,c4,c7.c9"' \
                   '"xattrs.selinux.high_sens":4' \
                   '"xattrs.selinux.low_cat": { $size: 2 }' \
                   '"xattrs.selinux.low_cat.0.f":1' \
                   '"xattrs.selinux.low_cat.0.l":1' \
                   '"xattrs.selinux.low_cat.1.f":4' \
                   '"xattrs.selinux.low_cat.1.l":4' \
                   '"xattrs.selinux.high_cat": { $size: 3 }' \
                   '"xattrs.selinux.high_cat.0.f":1' \
                   '"xattrs.selinux.high_cat.0.l":1' \
                   '"xattrs.selinux.high_cat.1.f":4' \
                   '"xattrs.selinux.high_cat.1.l":4' \
                   '"xattrs.selinux.high_cat.2.f":7' \
                   '"xattrs.selinux.high_cat.2.l":9'
}


################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_selinux_context_decomposition
    test_selinux_simple_range
    test_selinux_complete_range
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
