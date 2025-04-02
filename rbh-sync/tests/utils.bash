#!/usr/bin/env bash

# This file is part of rbh-fsevents.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                           RBH-SYNC UTILITIES                                 #
################################################################################

get_binary_id_from_mongo()
{
    local entry="$1"

    entry="$(echo "$entry" | tr -d '\n')"
    entry="BinData${entry#*BinData}"
    entry="${entry%)*})"

    echo "$entry"
}

check_id_parent_id()
{
    for entry in "$@"; do
        entry_ns_parent="$(do_db get "$testdb" \
                           '"ns.xattrs.path": "'$entry'"' \
                           '"ns.parent":1, "_id":0')"
        entry_ns_parent_id="$(get_binary_id_from_mongo "$entry_ns_parent")"

        parent_path="$(dirname $entry)"
        parent="$(do_db get "$testdb" \
                  '"ns.xattrs.path": "'$parent_path'"' '"_id":1')"
        parent_id="$(get_binary_id_from_mongo "$parent")"

        if [[ "$entry_ns_parent_id" != "$parent_id" ]]; then
            error "Id and parent Id differ for $entry, got " \
                  "$entry_ns_parent_id and $parent_id"
        fi
    done
}
