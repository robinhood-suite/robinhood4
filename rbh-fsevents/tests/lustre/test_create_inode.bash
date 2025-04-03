#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_create_entry()
{
    local entry="test_entry"
    create_entry "$entry"

    invoke_rbh-fsevents

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries != $count ]]; then
        error "There should be only $count entries in the database"
    fi

    verify_statx "$entry"
    verify_lustre "$entry"

    find_attribute '"xattrs.nb_children": '"$NB_ENTRY"''
}

test_create_two_entries()
{
    local entry1="test_entry1"
    local entry2="test_entry2"
    create_entry "$entry1"

    create_filled_entry "$entry2"

    invoke_rbh-fsevents

    verify_statx "$entry1"
    verify_lustre "$entry1"
    verify_statx "$entry2"
    verify_lustre "$entry2"

    find_attribute '"xattrs.nb_children": '"$(($NB_ENTRY * 2))"''
}
