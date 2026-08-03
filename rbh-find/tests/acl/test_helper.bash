#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_helper()
{
    rbh_find --help acl ||
        error "Find's helper with acl extension should have succeeded"

    rbh_find --help acl | grep "ACL" ||
        error "Find's helper with acl extension should have shown ACL predicates"

    rbh_find --help acl | grep "POSIX" ||
        error "Find's helper with acl extension should have shown POSIX predicates"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_helper)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
