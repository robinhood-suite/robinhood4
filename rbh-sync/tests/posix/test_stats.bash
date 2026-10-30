#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/posix_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid_stat_options()
{
    rbh_sync rbh:posix:$file rbh:$db:$testdb --stats --stats-interval blob &&
        error "Sync with invalid log interval should have failed"

    rbh_sync rbh:posix:$file rbh:$db:$testdb --stats --stats-interval -3 &&
        error "Sync with invalid log interval should have failed"

    return 0
}

find_expected_values()
{
    local output="$1"
    local entry_count="$2"
    local file_count="$3"

    echo "$output" | grep "rbh-sync" > /dev/null ||
        error "Should have found 'rbh-sync' mentionned, got '$output'"

    echo "$output" | grep "progress" | grep "$entry_count" > /dev/null ||
        error "Last log shown should have found '$entry_count' entries in total, got '$output'"

    echo "$output" | grep "directories seen" | grep "91" > /dev/null ||
        error "Last log shown should have found 91 directories in total, got '$output'"

    echo "$output" | grep "files seen" | grep "$file_count" > /dev/null ||
        error "Last log shown should have found '$file_count' files in total, got '$output'"

    echo "$output" | grep "links seen" | grep "1" > /dev/null ||
        error "Last log shown should have found 1 symlink in total, got '$output'"

    echo "$output" | grep "current speed" | grep "entries/sec" > /dev/null ||
        error "Logs should show the current speed in entries per second, got '$output'"
}

test_stats()
{
    mkdir -p {1..9}/{1..9}
    touch blob
    touch something
    ln -s something else

    local output="$(rbh_sync rbh:posix:. rbh:$db:$testdb --stats 2>&1)"
    find_expected_values "$output" 94 2

    rbh_sync rbh:posix:. rbh:$db:$testdb --stats --stats-file logs.txt
    output="$(cat logs.txt)"
    # 94 + 1 because of logs.txt
    find_expected_values "$output" 95 3

    output="$(rbh_sync rbh:posix:. rbh:$db:$testdb \
                --stats --stats-file syslog 2>&1)"
    # Need to remove up to the first space because with syslog we remove the
    # header (using '-o cat'), and when printing to stderr, syslog adds an
    # initial "RobinHood4[<pid>]: " to the string. So we have to remove that
    # part to make sure the strings match exactly.
    output="$(echo "${output#*: }")"
    journalctl --sync
    local syslog_output="$(journalctl -t RobinHood4 -o cat)"

    # Clean the journalctl logs to ensure future tests do not stumble on those
    journalctl -t RobinHood4 --rotate
    journalctl -t RobinHood4 --vacuum-size=1

    find_expected_values "$output" 95 3
    if [ "$output" != "$syslog_output" ]; then
        error "Syslog output and stderr should have been the same," \
              "got syslog '$syslog_output' and stderr '$output'"
    fi
}

test_mpi_stats()
{
    mkdir -p {1..9}/{1..9}
    touch blob
    touch something
    ln -s something else

    local output="$(rbh_sync rbh:posix-mpi:. rbh:$db:$testdb --stats 2>&1)"

    # Each MPI process outputs a stats block at the end of the command, but
    # mpirun does not reliably forward the output of the ranks that exit
    # quickly, so blocks can be missing from the merged output. Therefore, we
    # can only check the block outputed have a valid formatting, and that the
    # database has the correct amount of stats recorded.

    # mpirun launches one process per slot, computed from the machine's full
    # topology, which differs from nproc on the CI runners: the job shell is
    # pinned to a single CPU there, so nproc returns 1. Ask mpirun how many
    # processes it launches.
    local process_count=$(mpirun --allow-run-as-root hostname | wc -l)
    local stats_count=$(do_db count_stats $testdb)
    if (( stats_count != process_count )); then
        error "'$process_count' stats should have been sync'ed, got '$stats_count'"
    fi

    echo "$output" | grep "rbh-sync" > /dev/null ||
        error "Should have found 'rbh-sync' mentionned, got '$output'"

    echo "$output" | grep "entries scanned" > /dev/null ||
        error "Logs should show the progress of the scan, got '$output'"

    echo "$output" | grep "directories seen" > /dev/null ||
        error "Logs should show the number of directories seen, got '$output'"

    echo "$output" | grep "files seen" > /dev/null ||
        error "Logs should show the number of files seen, got '$output'"

    echo "$output" | grep "links seen" > /dev/null ||
        error "Logs should show the number of links seen, got '$output'"

    echo "$output" | grep "current speed" | grep "entries/sec" > /dev/null ||
        error "Logs should show the current speed in entries per second, got '$output'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=()

if [[ $WITH_MPI == true ]]; then
    tests+=(test_mpi_stats)
else
    # Run the 'test_invalid_stat_options' without MPI because there can be
    # an OpenMPI abort-path deadlock, so if every mpi process fails as the test
    # expects, it can sometimes hang the full script up until timeout, at which
    # point Meson aborts the script altogether
    tests+=(test_invalid_stat_options test_stats)
fi

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
