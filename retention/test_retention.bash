#!/usr/bin/env bash

# This file is part of RobinHood4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

function test_parsing
{
    rbh_retention &&
        error "command should have failed because of missing parameters"

    rbh_retention blob &&
        error "command should have failed because 'blob' does not exist"

    touch blob

    rbh_retention blob blob2 &&
        error "command should have failed because 'blob2' is not a valid date"

    rbh_retention blob 123 blob2 &&
        error "command should have failed because of more parameters than necessary"

    rbh_retention --relative &&
        error "command should have failed because of missing parameters"

    return 0
}

function test_set_retention
{
    local dir="dir"
    local file="file"
    local symlink="symlink"

    touch $file
    mkdir $dir
    ln -s $file $symlink

    local date="$(date -d "now - 5 minutes" +%s)"
    rbh_retention $dir "now - 5 minutes"
    local attr="$(getfattr -n user.expires --only-values $dir)"
    if [ "$attr" != "$date" ]; then
        error "'user.expires' value should be set at '$date' for '$dir'"
    fi

    rbh_retention $dir "5 minutes" --relative
    local attr="$(getfattr -n user.expires --only-values $dir)"
    if [ "$attr" != "+300" ]; then
        error "'user.expires' value should be set at '+300' for '$dir'"
    fi

    rbh_retention $file --relative "3 hours"
    local attr="$(getfattr -n user.expires --only-values $file)"
    if [ "$attr" != "+10800" ]; then
        error "'user.expires' value should be set at '+10800' for '$file'"
    fi

    rbh_retention --relative $symlink "1 day"
    local attr="$(getfattr -n user.expires --only-values $symlink)"
    if [ "$attr" != "+86400" ]; then
        error "'user.expires' value should be set at '+86400' for '$symlink'"
    fi
}

function test_get_retention
{
    local dir="dir"
    local file="file"
    local symlink="symlink"

    touch $file
    mkdir $dir
    ln -s $file $symlink

    local date="$(date -d "now - 5 minutes" +%s)"
    setfattr -n user.expires -v "$date" $dir
    local attr="$(rbh_retention $dir)"
    local date2attr="$(date -d "@$date")"
    if [[ "$attr" != *"$date2attr"* ]]; then
        error "'$date' value should have been converted to '$date2attr' for '$dir'"
    fi

    date="$(($(date -d "10 minutes" +%s) - $(date +%s)))"
    setfattr -n user.expires -v "+$date" $dir
    local attr="$(rbh_retention $dir)"
    if [[ "$attr" != *"10 minute(s)"* ]]; then
        error "'$date' value should have been converted to '10 minutes' for '$dir'"
    fi

    date="$(($(date -d "12 hours" +%s) - $(date +%s)))"
    setfattr -n user.expires -v "+$date" $file
    local attr="$(rbh_retention $file)"
    if [[ "$attr" != *"12 hour(s)"* ]]; then
        error "'$date' value should have been converted to '12 hours' for '$file'"
    fi

    date="$(($(date -d "2 days" +%s) - $(date +%s)))"
    setfattr -n user.expires -v "+$date" $symlink
    local attr="$(rbh_retention $symlink)"
    if [[ "$attr" != *"2 day(s)"* ]]; then
        error "'$date' value should have been converted to '2 days' for '$symlink'"
    fi

    date="$(($(date -d "2 days + 1 hour" +%s) - $(date +%s)))"
    setfattr -n user.expires -v "+$date" $file
    local attr="$(rbh_retention $file)"
    if [[ "$attr" != *"2 day(s) and 1 hour(s)"* ]]; then
        error "'$date' value should have been converted to '2 days and 1 hours' for '$file'"
    fi

    date="$(($(date -d "2 days + 2 hours + 5 minutes" +%s) - $(date +%s)))"
    setfattr -n user.expires -v "+$date" $file
    local attr="$(rbh_retention $file)"
    if [[ "$attr" != *"2 day(s), 2 hour(s) and 5 minute(s)"* ]]; then
        error "'$date' value should have been converted to '2 days, 2 hours and 5 minutes' for '$file'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_parsing test_set_retention test_get_retention)

tmpdir=$(mktemp --directory)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
