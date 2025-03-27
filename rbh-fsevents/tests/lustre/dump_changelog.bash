#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_dump()
{
    local file="test_file"
    local dump_file="/tmp/dump_file"

    touch $file
    rm $file

    local changelogs=$(lfs changelog $LUSTRE_MDT | cut -d' ' -f2)

    diff <(lfs changelog $LUSTRE_MDT | awk '{ print substr($2, 3) }') \
         <(rbh_fsevents --dump "-" --enrich rbh:lustre:"$LUSTRE_DIR" \
               src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" |
               awk '{ print substr($3, 3) }')

    local output=$(rbh_fsevents --dump "-" --enrich rbh:lustre:"$LUSTRE_DIR" \
                       src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb")

    rbh_fsevents --dump "$dump_file" --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"

    diff <(echo "$output") $dump_file

    rm $dump_file
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_dump)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'; \
      rm /tmp/dump_file" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
