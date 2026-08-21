#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_delete()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find rbh:$db:$testdb > /dev/null
    rbh_find rbh:$db:$testdb > /dev/null

    rbh_report rbh:$db:$testdb \
        --group-by "statx.uid" \
        --output "sum(statx.size)" > /dev/null

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '4'" \
                  "Total log count: '7'"

    rbh_log rbh:$db:$testdb --sync 3 --delete

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '4'"

    rbh_log rbh:$db:$testdb --gc 7 --delete

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '4'"

    rbh_log rbh:$db:$testdb --last 1 --delete

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '3'"

    rbh_log rbh:$db:$testdb --first 1 --delete

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '0'" \
                  "Total log count: '2'"

    rbh_log rbh:$db:$testdb --find 42 --delete

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '0'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '0'" \
                  "Total log count: '0'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_delete)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
