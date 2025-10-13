#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

setup_script()
{
    local expected=$1

    export tmp_gdb_script=$(mktemp)
    cat << EOF > "$tmp_gdb_script"
define hook-stop
  if \$run_python_after_return
    python
import subprocess, gdb
proc = subprocess.run("lfs changelog $LUSTRE_MDT | wc -l", shell=True,
                      stdout=subprocess.PIPE, check=True)
gdb.execute("set \$nb_changelog = %d" % int(proc.stdout.decode()))
    end

    if (\$nb_changelog != \$expected)
      quit 1
    end
  end
end

set \$expected = $expected

break lustre_changelog_ack_batch
commands
  break llapi_changelog_clear
  commands
      set \$run_python_after_return = 1
      finish
      continue
  end
  continue
end

run
EOF
}

test_ack()
{
    touch entry
    touch entry2

    setup_script 2

    # We use a batch_size of 2 because a CREATE changelog generate fsevents for
    # the entry and its parents, so 2 entries
    DEBUGINFOD_URLS="" gdb --batch -x "$tmp_gdb_script" \
        --args $__rbh_fsevents -w 1 -b 2 --enrich rbh:lustre:"$LUSTRE_DIR" \
            src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"
}

test_ack_no_dedup()
{
    touch entry

    setup_script 1

    DEBUGINFOD_URLS="" gdb --batch -x "$tmp_gdb_script" \
        --args $__rbh_fsevents -w 1 -b 0 --enrich rbh:lustre:"$LUSTRE_DIR" \
            src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_ack test_ack_no_dedup)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
