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

test_config_specified() {
    echo -e "alias: \n  a1: \"-pool namer -layout-pattern default \
            -stripe-count default\"" > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias a1
    if [ $? -ne 0 ]; then
        error "The command failed when specifying a valid config file."
    fi
}

test_config_default() {
    LOCK_FILE="/tmp/rbh_conf_lock"
    (

    flock -x 122
    sudo bash -c 'rm -rf /etc/robinhood4'
    sudo bash -c 'mkdir -p /etc/robinhood4'
    sudo bash -c 'echo -e "alias: \n  a1: \"-pool namer -layout-pattern \
                 default -stripe-count default\"" > \
                 /etc/robinhood4/rbh_conf.yaml'

    rbh-lfind "rbh:mongo:$testdb" --alias a1
    if [ $? -ne 0 ]; then
        error "The command failed when using the default config file."
    fi
    sudo bash -c 'rm -rf /etc/robinhood4'
    ) 122>>"$LOCK_FILE"
}

test_multiple_alias() {
    echo -e "alias: \n  a1: \"-pool namer -layout-pattern default\"\n  a2: \
            \"-print\"" > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias a2 --alias \
        a1 --alias a2
    if [ $? -ne 0 ]; then
        error "Multiple alias resolution in a single command failed."
    fi
}

test_recursive_alias() {
    echo -e "alias: \n  a1: \"--alias a2\"\n  a2: \"--alias \
            a3\"\n  a3: \"-print\"" > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias a1
    if [ $? -ne 0 ]; then
        error "Recursive alias resolution failed."
    fi
}

test_recursive_alias_loop() {
    echo -e "alias: \n  a1: \"--alias a2\"\n  a2: \"--alias a1\" \
            " > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias a1
    if [ $? -eq 0 ]; then
        error "Alias loop was not detected."
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_config_specified test_config_default test_multiple_alias
                  test_recursive_alias test_recursive_alias_loop)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'; rm -rf /etc/robinhood4" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
