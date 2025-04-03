#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_rm_same_batch()
{
    local entry="test_entry"
    create_entry $entry
    rm_entry $entry

    invoke_rbh-fsevents

    # The only entry in the database should be the mount point, as the created
    # entry was immediately deleted and should have been ignored
    local entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    find_attribute '"xattrs.nb_children": 0'
}

test_rm_different_batch()
{
    local entry="test_entry"
    create_entry $entry

    invoke_rbh-fsevents

    find_attribute '"xattrs.nb_children": 1'

    clear_changelogs "$LUSTRE_MDT" "$userid"
    rm_entry $entry

    invoke_rbh-fsevents

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    find_attribute '"xattrs.nb_children": 0'
}
