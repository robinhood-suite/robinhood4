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

test_regex()
{
    mkdir -p test/toto test/alice
    touch test/toto/blob2025 test/alice/BLob1980
    touch test/toto/lorem.txt

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -regex ".*" | sort |
        difflines "/" "/test" "/test/alice" "/test/alice/BLob1980" "/test/toto" \
            "/test/toto/blob2025" "/test/toto/lorem.txt"
    rbh_find "rbh:$db:$testdb" -regex "^/test/toto/.*" | sort |
        difflines "/test/toto/blob2025" "/test/toto/lorem.txt"
    rbh_find "rbh:$db:$testdb" -regex "^/test/toto/.*\.txt$" | sort |
        difflines "/test/toto/lorem.txt"
    rbh_find "rbh:$db:$testdb" -regex ".*/blob.*[0-9](\..*)?$" | sort |
        difflines "/test/toto/blob2025"
    rbh_find "rbh:$db:$testdb" -regex ".*/BLob.*" | sort |
        difflines "/test/alice/BLob1980"
}

test_iregex()
{
    mkdir -p test/tOtO test/alice
    touch test/tOtO/blob2025 test/alice/BLob1980
    touch test/tOtO/lorem.txt

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -iregex "^/test/toto/.*" | sort |
        difflines "/test/tOtO/blob2025" "/test/tOtO/lorem.txt"
    rbh_find "rbh:$db:$testdb" -regex "^/test/toto/.*" | sort |
        difflines

    rbh_find "rbh:$db:$testdb" -iregex "^/test/toto/.*\.txt$" | sort |
        difflines "/test/tOtO/lorem.txt"
    rbh_find "rbh:$db:$testdb" -iregex ".*/blob.*[0-9](\..*)?$" | sort |
        difflines "/test/alice/BLob1980" "/test/tOtO/blob2025"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_regex test_iregex)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
