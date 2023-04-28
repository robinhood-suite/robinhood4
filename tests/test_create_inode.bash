#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_create_entry()
{
    local entry="test_entry"
    create_entry "$entry"

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    find_attribute "\"ns.name\":\"$entry\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry\""
}

test_create_two_entries()
{
    local entry1="test_entry1"
    local entry2="test_entry2"
    create_entry "$entry1"

    invoke_rbh-fsevents

    find_attribute "\"ns.name\":\"$entry1\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry1\""

    clear_changelogs
    create_entry "$entry2"

    invoke_rbh-fsevents

    find_attribute "\"ns.name\":\"$entry1\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry1\""
    find_attribute "\"ns.name\":\"$entry2\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry2\""
}

test_create_entry_check_statx_attr()
{
    local entry="test_entry"
    create_filled_entry "$entry"

    invoke_rbh-fsevents

    find_attribute "\"ns.name\":\"$entry\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry\""
    find_attribute "\"statx.atime.sec\":NumberLong($(statx +%X "$entry"))" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.atime.nsec\":0" "\"ns.name\":\"$entry\""
    find_attribute "\"statx.ctime.sec\":NumberLong($(statx +%Z "$entry"))" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.ctime.nsec\":0" "\"ns.name\":\"$entry\""
    find_attribute "\"statx.mtime.sec\":NumberLong($(statx +%Y "$entry"))" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.mtime.nsec\":0" "\"ns.name\":\"$entry\""
    find_attribute "\"statx.btime.nsec\":0" "\"ns.name\":\"$entry\""
    find_attribute "\"statx.blksize\":$(statx +%o "$entry")" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.blocks\":NumberLong($(statx +%b "$entry"))" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.nlink\":$(statx +%h "$entry")" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.ino\":NumberLong(\"$(statx +%i "$entry")\")" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.gid\":$(statx +%g "$entry")" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.uid\":$(statx +%u "$entry")" \
                   "\"ns.name\":\"$entry\""
    find_attribute "\"statx.size\":NumberLong($(statx +%s "$entry"))" \
                   "\"ns.name\":\"$entry\""

    local raw_mode="$(statx +%f "$entry" 16)"
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))
    find_attribute "\"statx.type\":$type" "\"ns.name\":\"$entry\""
    find_attribute "\"statx.mode\":$mode" "\"ns.name\":\"$entry\""

    # TODO: I do not know how to retrieve these values: btime (stat doesn't give
    # what is recorded in the db, maybe a bug) and device numbers

    #find_attribute "\"statx.btime.sec\":NumberLong($(statx +%W '$entry'))"
    #find_attribute "\"statx.dev.major\":$(statx +%d '$entry')"
    #find_attribute "\"statx.dev.minor\":$(statx +%d '$entry')"
}
