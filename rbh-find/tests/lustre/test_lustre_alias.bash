#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_config_specified()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer -layout-pattern default -stripe-count default"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a1)
    pattern="\-pool namer \-layout-pattern default \-stripe-count default"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command output does not match the expected output."
    fi
}

test_config_default()
{
    mkdir -p /etc/robinhood4.d
    cat > /etc/robinhood4.d/default.yaml <<EOF
alias:
   a1: "-pool namer -layout-pattern default -stripe-count default"
EOF

    # Do not use global default config
    unset RBH_CONFIG_PATH
    command_output=$(rbh_find --dry-run "rbh:$db:$testdb" --alias a1)
    pattern="\-pool namer \-layout-pattern default \-stripe-count default"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command failed when using the default config file."
    fi
    rm -rf /etc/robinhood4.d/default.yaml
}

test_multiple_alias()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer -layout-pattern default"
   a2: "-print"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a2 --alias a1 --alias a2)
    pattern="\-print \-pool namer \-layout-pattern default \-print"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "Multiple alias resolution in a single command failed."
    fi
}

test_recursive_alias()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "--alias a2"
   a2: "--alias a3"
   a3: "-print"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a1)
    pattern="\-print"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "Recursive alias resolution failed."
    fi
}

test_recursive_alias_loop()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "--alias a2"
   a2: "--alias a1"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a1 2>&1 || true)
    pattern="\[ERROR\] Infinite loop detected for alias 'a1'.Execution stopped."

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "Alias loop was not detected."
    fi
}

test_alias_repeated()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer -print"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a1 --alias a1)
    pattern="\-pool namer \-print \-pool namer \-print"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command failed when using the same alias twice."
    fi
}

test_aliases_in_single_argument()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer"
   a2: "-print"
   a3: "-print"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a1,a2,a3)
    pattern="\-pool namer \-print \-print"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command failed when using aliases separated commas"
    fi
}

test_aliases_in_single_argument_recursive()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer --alias a2,a3"
   a2: "-print"
   a3: "-print"
EOF

    command_output=$(rbh_find --config test_conf.yaml --dry-run \
                    "rbh:$db:$testdb" --alias a1)
    pattern="\-pool namer \-print \-print"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command failed when using aliases separated commas"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_config_specified test_config_default test_multiple_alias
                  test_recursive_alias test_recursive_alias_loop
                  test_alias_repeated test_aliases_in_single_argument
                  test_aliases_in_single_argument_recursive)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'; rm -rf /etc/robinhood4" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
