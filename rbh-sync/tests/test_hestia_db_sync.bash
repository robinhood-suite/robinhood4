#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

hestia_obj()
{
    hestia object --verbosity 1 $@
}

hestia_get_attr()
{
    local obj="$1"
    local attr="$2"

    hestia_obj read "$obj" | grep "$attr" | head -n 1 | cut -d':' -f2 |
        cut -d',' -f1 | xargs
}

find_time_attribute()
{
    local category="$1"
    local time="$2"
    local object="$3"

    find_attribute '"statx.'$category'.sec":NumberLong("'$time'")' \
                   '"ns.name":"'$object'"'
}

test_sync()
{
    local obj1=$(hestia_obj create)
    local obj2=$(hestia_obj create)

    hestia_obj put_data $obj1 --file /etc/hosts
    hestia_obj put_data $obj2 --file /etc/hosts

    rbh_sync "rbh:hestia:" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$obj1'"'
    find_attribute '"ns.xattrs.path":"'$obj1'"'
    find_attribute '"ns.name":"'$obj2'"'
    find_attribute '"ns.xattrs.path":"'$obj2'"'
}

test_put_data()
{
    local obj=$(hestia_obj create)
    hestia_obj put_data $obj --file /etc/hosts

    rbh_sync "rbh:hestia:" "rbh:$db:$testdb"

    local size=$(stat -c %s /etc/hosts)

    find_attribute '"statx.size":'$size '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.extents":'$size '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.index": 0' '"ns.name":"'$obj'"'

    local time=$(hestia_get_attr "$obj" "creation_time")

    find_time_attribute "btime" "$time" "$obj"

    time=$(hestia_get_attr "$obj" "content_accessed_time")

    find_time_attribute "atime" "$time" "$obj"
    find_time_attribute "ctime" "$time" "$obj"
    find_time_attribute "mtime" "$time" "$obj"
}

test_copy()
{
    local obj=$(hestia_obj create)
    hestia_obj put_data $obj --file /etc/hosts
    hestia_obj copy_data $obj --source 0 --target 1

    rbh_sync "rbh:hestia:" "rbh:$db:$testdb"

    local size=$(stat -c %s /etc/hosts)

    find_attribute '"statx.size":'$size '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.extents":'$size '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.index": 0' '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.index": 1' '"ns.name":"'$obj'"'
}

test_move()
{
    local obj=$(hestia_obj create)
    hestia_obj put_data $obj --file /etc/hosts
    hestia_obj move_data $obj --source 0 --target 1

    rbh_sync "rbh:hestia:" "rbh:$db:$testdb"

    local size=$(stat -c %s /etc/hosts)

    find_attribute '"statx.size":'$size '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.extents":'$size '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.index": {$ne: 0}' '"ns.name":"'$obj'"'
    find_attribute '"xattrs.tiers.index": 1' '"ns.name":"'$obj'"'
}

test_user_attributes()
{
    local obj1=$(hestia_obj create)
    hestia_obj put_data $obj1 --file /etc/hosts
    local obj2=$(hestia_obj create)
    hestia_obj put_data $obj2 --file /etc/hosts

    hestia metadata update --id_fmt=parent_id --input_fmt=key_value $obj1 <<< \
        data.my_keys=alicia
    hestia metadata update --id_fmt=parent_id --input_fmt=key_value $obj1 <<< \
        data.blob=blob
    hestia metadata update --id_fmt=parent_id --input_fmt=key_value $obj2 <<< \
        data.mic=test

    rbh_sync "rbh:hestia:" "rbh:$db:$testdb"

    find_attribute '"xattrs.user_metadata.my_keys":"alicia"' \
                   '"ns.name":"'$obj1'"'
    find_attribute '"xattrs.user_metadata.blob":"blob"' '"ns.name":"'$obj1'"'
    find_attribute '"xattrs.user_metadata.mic":"test"' '"ns.name":"'$obj2'"'

    find_attribute '"xattrs.user_metadata.keys": { $exists : false }' \
        '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.user_metadata.blob": { $exists : false }' \
        '"ns.name":"'$obj2'"'
    find_attribute '"xattrs.user_metadata.mic": { $exists : false }' \
        '"ns.name":"'$obj1'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

hestia_teardown()
{
    rm -rf $HOME/.cache/hestia
}

declare -a tests=(test_sync test_put_data test_copy test_move
                  test_user_attributes)

test_teardown=hestia_teardown
run_tests ${tests[@]}
