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

test_links()
{
    touch file blob
    mkdir dir
    ln file hard_link
    ln -s file soft_link

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -links 1 | sort |
        difflines "/blob" "/soft_link"
    rbh_find "rbh:mongo:$testdb" -links +2 | sort | difflines "/"
    rbh_find "rbh:mongo:$testdb" -links -3 | sort |
        difflines "/blob" "/dir" "/file" "/hard_link" "/soft_link"

    rbh_find "rbh:mongo:$testdb" -links 1 -o -links +2 | sort |
        difflines "/" "/blob" "/soft_link"
    rbh_find "rbh:mongo:$testdb" -links 1 -a -links +2 | sort | difflines
    rbh_find "rbh:mongo:$testdb" -not -links 2 | sort |
        difflines "/" "/blob" "/soft_link"
}

test_lname()
{
    touch file
    touch blob

    ln -s file file_link
    ln -s file file_link_bis
    ln -s blob blob_link

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -lname file | sort |
        difflines "/file_link" "/file_link_bis"

    rbh_find "rbh:mongo:$testdb" -lname toto | sort | difflines

    rbh_find "rbh:mongo:$testdb" -lname '*' | sort |
        difflines "/blob_link" "/file_link" "/file_link_bis"

    rbh_find "rbh:mongo:$testdb" -not -lname file | sort |
        difflines "/" "/blob" "/blob_link" "/file"

    rbh_find "rbh:mongo:$testdb" -lname blob -o -lname file | sort |
        difflines "/blob_link" "/file_link" "/file_link_bis"
}

test_ilname()
{
    touch file
    touch blob

    ln -s file file_link
    ln -s file file_link_bis
    ln -s blob blob_link

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -lname Blob | sort | difflines
    rbh_find "rbh:mongo:$testdb" -ilname Blob | sort |
        difflines "/blob_link"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_links test_lname test_ilname)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
