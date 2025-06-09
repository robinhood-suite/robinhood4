#!/usr/bin/env bash
#
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if [[ "$EUID" -ne 0 ]]; then
    echo "This setup script must be run as root"
    exit 1
fi

test_dir=$(dirname "$(realpath "${BASH_SOURCE[0]}")")
source "${test_dir}/framework.bash"

set -e

export PATH="$PATH:/usr/lib64/lustre/tests"

HSM_DIR=/tmp/hsm
LUSTRE_ROOT=/mnt/lustre

which llmount.sh >/dev/null ||
    error "Missing 'llmount.sh', install the package 'lustre-tests'"

systemctl start mongod ||
    error "Failed to start mongod"

export OSTCOUNT=4
export MDSCOUNT=4
llmount.sh

lctl pool_new lustre.test_pool1
lctl pool_new lustre.test_pool2
lctl pool_new lustre.test_pool3
lctl pool_add lustre.test_pool1 OST[0]
lctl pool_add lustre.test_pool2 OST[1]
lctl pool_add lustre.test_pool3 OST[2]

mkdir -p "$HSM_DIR"
lctl set_param mdt.*.hsm_control=enabled
lhsmtool_posix -q --daemon --hsm-root "$HSM_DIR" --archive=1 "$LUSTRE_ROOT"
