#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_config_specified() {
    echo -e "alias: \n  pilo: \"-pool namer -layout-pattern default \
            -stripe-count default\"" > test_conf.yaml
    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias pilo
    if [ $? -ne 0 ]; then
        error "The command failed when specifying a valid config file."
    fi
}

test_config_default() {
    sudo bash -c "mkdir -p /etc/robinhood4"
    sudo bash -c 'echo -e "alias: \n  pilo: \"-pool namer -layout-pattern \
                 default -stripe-count default\"" > \
                 /etc/robinhood4/rbh_conf.yaml'

    rbh-lfind "rbh:mongo:$testdb" --alias pilo
    if [ $? -ne 0 ]; then
        error "The command failed when using the default config file."
    fi
}

test_alias_position() {
    echo -e "alias: \n  pilo: \"-pool namer -layout-pattern default\"\n  polo: \
            \"-print\"" > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias polo --alias \
        pilo --alias polo
    if [ $? -ne 0 ]; then
        error "Multiple alias resolution in a single command failed."
    fi

}

test_recursive_alias() {
    echo -e "alias: \n  pilo: \"--alias polo\"\n  polo: \"--alias \
            fast-access\"\n  fast-access: \"-print\"" > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb"
    if [ $? -ne 0 ]; then
        error "Recursive alias resolution failed."
    fi
}

test_recursive_alias_loop() {
    echo -e "alias: \n  pilo: \"--alias polo\"\n  polo: \"--alias pilo\" \
            " > test_conf.yaml

    rbh-lfind --config test_conf.yaml "rbh:mongo:$testdb" --alias polo
    if [ $? -ne 0 ]; then
        error "Alias loop was not detected."
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_config_specified test_config_default test_alias_position
                  test_recursive_alias test_recursive_alias_loop)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'; rm -rf /etc/robinhood4" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
