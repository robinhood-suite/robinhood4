#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/selinux_utils.bash

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"


################################################################################
#                                    TESTS                                     #
################################################################################

test_selinux_fields_exist()
{
    require_selinux

    touch file_context
    set_selinux_range_or_skip "s0:c0" file_context

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file_context"' \
                   '"xattrs.selinux.context": { $exists: true }' \
                   '"xattrs.selinux.user": { $exists: true }' \
                   '"xattrs.selinux.role": { $exists: true }' \
                   '"xattrs.selinux.type": { $exists: true }' \
                   '"xattrs.selinux.range": { $exists: true }' \
                   '"xattrs.selinux.low_sens": { $exists: true }' \
                   '"xattrs.selinux.high_sens": { $exists: true }' \
                   '"xattrs.selinux.low_cat": { $exists: true }' \
                   '"xattrs.selinux.high_cat": { $exists: true }' 
}


test_selinux_context_decomposition()
{
    local ctx
    local seuser
    local serole
    local setype
    local serange

    require_selinux

    touch file
    set_selinux_range_or_skip "s0:c0" file 

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

    require_selinux

    touch file 
    set_selinux_range_or_skip "s0:c0" file

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.range":"s0:c0"' \
                   '"xattrs.selinux.low_sens":0' \
                   '"xattrs.selinux.high_sens":0' \
                   '"xattrs.selinux.low_cat": { $elemMatch: { f: 0, l: 0 } }' \
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: 0, l: 0 } }' 

    count=$(count_documents '"ns.xattrs.path":"/file"' \
                            '"xattrs.selinux.high_cat": { $elemMatch: { f: { $lte: 1 }, l: { $gte: 1 } } }')

    if (( count != 0 )); then
        error "SELinux low range s0:c0 should not set category c1"
    fi
}


test_selinux_complete_range()
{
    local count

    require_selinux

    touch file 
    set_selinux_range_or_skip "s0:c1,c4-s4:c1,c4,c7.c9" file

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.range":"s0:c1,c4-s4:c1,c4,c7.c9"' \
                   '"xattrs.selinux.low_sens":0' \
                   '"xattrs.selinux.high_sens":4' \
                   '"xattrs.selinux.low_cat": { $elemMatch: { f: 1, l: 1 } }' \
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: 4, l: 4 } }' 

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: 1, l: 1 } }' 

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: 4, l: 4 } }' 

    find_attribute '"ns.xattrs.path":"/file"' \
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: 7, l: 9 } }' 


    count=$(count_documents '"ns.xattrs.path":"/file"' \
                            '"xattrs.selinux.high_cat": { $elemMatch: { f: { $lte: 2 }, l: { $gte: 2 } } }')

    if (( count != 0 )); then
        error "SELinux low range c1,c4 should not set category c2"
    fi

    count=$(count_documents '"ns.xattrs.path":"/file"' \
                            '"xattrs.selinux.high_cat": { $elemMatch: { f: { $lte: 6 }, l: { $gte: 6 } } }')

    if (( count != 0 )); then
        error "SELinux low range c1,c4,c7.c9 should not set category c6"
    fi
}

test_selinux_range_filter()
{
    local count

    require_selinux

    touch bad
    touch good

    set_selinux_range_or_skip "s0:c1,c4-s4:c1,c4,c7.c9" good
    set_selinux_range_or_skip "s0:c1,c4" bad

    rbh_sync_selinux "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"/good"' \
                   '"xattrs.selinux.high_sens": { $gte: 4 }'\
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: { $lte: 8 }, l: { $gte: 8 } } }'

    count=$(count_documents '"ns.xattrs.path":"/bad"' \
                   '"xattrs.selinux.high_sens": { $gte: 4 }'\
                   '"xattrs.selinux.high_cat": { $elemMatch: { f: { $lte: 8 }, l: { $gte: 8 } } }')

    if (( count != 0 )); then
        error "SELinux level s0:c1,c4 should not satisfy target level s4:c8"
    fi
}



################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(
    test_selinux_fields_exist
    test_selinux_context_decomposition
    test_selinux_simple_range
    test_selinux_complete_range
    test_selinux_range_filter
)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
