#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/hestia_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

acceptance()
{
    local obj1=$(hestia object --verbosity 1 create blob1)
    local obj2=$(hestia object --verbosity 1 create blob2)
    local obj3=$(hestia object --verbosity 1 create blob3)

    hestia object put_data --file /etc/hosts $obj1

    hestia metadata update --id_fmt=parent_id --input_fmt=key_value $obj1 <<< \
        data.my_key=my_value

    hestia object put_data --file /etc/hosts $obj2

    hestia object copy_data $obj2 --source 0 --target 1
    hestia object copy_data $obj2 --source 1 --target 2
    hestia object release_data $obj2 --tier 1
    hestia object get_data --file /dev/null --tier 2 $obj2

    hestia object remove $obj3

    invoke_rbh_fsevents "rbh:$db:$testdb"

    local count=$(mongo $testdb --eval "db.entries.count()")
    if (( $count != 2 )); then
        error "There should be 2 entries in the DB"
    fi

    local size="$(stat -c %s /etc/hosts)"

    find_attribute '"ns.name":"'$obj1'"'
    find_attribute '"ns.xattrs.path":"'$obj1'"'

    find_attribute '"xattrs.tiers": { $size: 1 }' '"ns.name":"'$obj1'"'
    find_attribute '"xattrs.user_metadata": { "my_key": "my_value" }'
    find_attribute '"xattrs.tiers.index":"0"' '"ns.name":"'$obj1'"'

    find_attribute '"ns.name":"'$obj2'"'
    find_attribute '"ns.xattrs.path":"'$obj2'"'

    find_attribute '"xattrs.tiers": { $size: 2 }' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.tiers.index":"0"' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.tiers.index":"2"' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.tiers.index": { $ne: "1"}' '"ns.name":"'$obj2'"'

    find_attribute '"xattrs.tiers.extents.length": "'$size'"' \
                   '"xattrs.tiers.index": "0"' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.tiers.extents.offset": "0"' \
                   '"xattrs.tiers.index": "0"' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.tiers.extents.length": "'$size'"' \
                   '"xattrs.tiers.index": "2"' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.tiers.extents.offset": "0"' \
                   '"xattrs.tiers.index": "2"' '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.user_metadata": { }' '"ns.name":"'$obj2'"'

    local btime1=$(hestia_get_attr "$obj1" "creation_time")
    find_time_attribute "btime" "$btime1" "$obj1"
    local btime2=$(hestia_get_attr "$obj2" "creation_time")
    find_time_attribute "btime" "$btime2" "$obj2"

    local atime1=$(hestia_get_attr "$obj1" "content_accessed_time")
    find_time_attribute "atime" "$atime1" "$obj1"
    local atime2=$(hestia_get_attr "$obj2" "content_accessed_time")
    find_time_attribute "atime" "$atime2" "$obj2"

    local mtime1=$(hestia_get_attr "$obj1" "content_modified_time")
    find_time_attribute "mtime" "$mtime1" "$obj1"
    local mtime2=$(hestia_get_attr "$obj2" "content_modified_time")
    find_time_attribute "mtime" "$mtime2" "$obj2"

    local ctime1=$(hestia_get_attr "$obj1" "metadata_modified_time")
    find_time_attribute "ctime" "$ctime1" "$obj1"
    local ctime2=$(hestia_get_attr "$obj2" "metadata_modified_time")
    find_time_attribute "ctime" "$ctime2" "$obj2"

    find_attribute '"statx.size": NumberLong('$size')' '"ns.name":"'$obj1'"'
    find_attribute '"statx.size": NumberLong('$size')' '"ns.name":"'$obj2'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(acceptance)

sub_setup=hestia_setup
sub_teardown=hestia_teardown
run_tests "${tests[@]}"
