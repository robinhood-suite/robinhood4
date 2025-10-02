#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_many_output()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    # Generate more than 250 different entries with different uid
    do_db custom "$testdb" 'db.entries.find({"ns.name": "file"}).forEach(
        function(doc){for(let i = 1; i < 252; i++) {doc._id = new ObjectId();
            doc.statx.uid = i; db.entries.insert(doc);}})'
    rbh_report "rbh:$db:$testdb" --output "count()" --group-by "statx.uid"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_many_output)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
