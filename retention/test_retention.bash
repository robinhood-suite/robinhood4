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

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_parsing test_set_retention)

tmpdir=$(mktemp --directory)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
