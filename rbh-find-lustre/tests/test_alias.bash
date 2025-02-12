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

test_config_specified()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer -layout-pattern default -stripe-count default"
EOF

    command_output=$(rbh-lfind --config test_conf.yaml --dry-run \
                    "rbh:mongo:$testdb" --alias a1)
    pattern="\-pool namer \-layout-pattern default \-stripe-count default"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command output does not match the expected output."
    fi
}

test_config_default()
{
    LOCK_FILE="/tmp/rbh_conf_lock"

    (
    flock -x 122
    sudo bash -c 'rm -rf /etc/robinhood4.d'
    sudo bash -c 'mkdir -p /etc/robinhood4.d'
    sudo bash -c 'cat > /etc/robinhood4.d/rbh_conf.yaml <<EOF
alias:
   a1: "-pool namer -layout-pattern default -stripe-count default"
EOF'

    command_output=$(rbh-lfind --dry-run "rbh:mongo:$testdb" --alias a1)
    pattern="\-pool namer \-layout-pattern default \-stripe-count default"

    if ! echo "$command_output" | grep -q "$pattern"; then
        error "The command failed when using the default config file."
    fi
    sudo bash -c 'rm -rf /etc/robinhood4.d'
    ) 122>>"$LOCK_FILE"
}

test_multiple_alias()
{
    cat > test_conf.yaml <<EOF
alias:
   a1: "-pool namer -layout-pattern default"
   a2: "-print"
EOF

    command_output=$(rbh-lfind --config test_conf.yaml --dry-run \
                    "rbh:mongo:$testdb" --alias a2 --alias a1 --alias a2)
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

    command_output=$(rbh-lfind --config test_conf.yaml --dry-run \
                    "rbh:mongo:$testdb" --alias a1)
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

    command_output=$(rbh-lfind --config test_conf.yaml --dry-run \
                    "rbh:mongo:$testdb" --alias a1 2>&1 || true)
    pattern="\[ERROR\] Infinite loop detected for alias 'a1'.Execution stopped."

    if ! echo "$command_output" | grep -q "$pattern"; then
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
