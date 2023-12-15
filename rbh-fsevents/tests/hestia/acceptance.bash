#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../test_utils.bash
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

    invoke_rbh_fsevents "rbh:mongo:$testdb"

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

    mongo $testdb --eval "db.entries.find()"
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

    # The time attribute parser from Hestia is different for the two objects
    # because a put_data modifies the access and change time of an object, but a
    # metadata update modifies only the change time. However, both commands
    # create an update event in the feed, but since we cannot differentiate an
    # update to metadata from an update to the data, we cannot properly assign
    # the time in the event feed to one category, so it is assigned to both, and
    # we check different times here
    local atime1=$(hestia_get_attr "$obj1" "content_modified_time")
    find_time_attribute "atime" "$atime1" "$obj1"
    local atime2=$(hestia_get_attr "$obj2" "content_accessed_time")
    find_time_attribute "atime" "$atime2" "$obj2"

    local ctime1=$(hestia_get_attr "$obj1" "content_modified_time")
    find_time_attribute "ctime" "$ctime1" "$obj1"
    local ctime2=$(hestia_get_attr "$obj2" "content_modified_time")
    find_time_attribute "ctime" "$ctime2" "$obj2"

    find_attribute '"statx.size": NumberLong('$size')' '"ns.name":"'$obj1'"'
    find_attribute '"statx.size": NumberLong('$size')' '"ns.name":"'$obj2'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(acceptance)

run_tests hestia_setup hestia_teardown "${tests[@]}"
