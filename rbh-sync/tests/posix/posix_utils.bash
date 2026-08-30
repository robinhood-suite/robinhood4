#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"

rbh_sync_posix()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync "rbh:posix-mpi:$1" "$2" ${@:3}
    else
        rbh_sync "rbh:posix:$1" "$2" ${@:3}
    fi
}

rbh_sync_posix_one()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync -o "rbh:posix-mpi:$1" "$2" ${@:3}
    else
        rbh_sync -o "rbh:posix:$1" "$2" ${@:3}
    fi
}

set_permission()
{
    local path=$1
    local sign=$2

    while [[ "$path" != "/home" ]] && [[ "$path" != "/" ]]; do
        if [[ "$sign" == "+" ]]; then
            chmod o+rx $path
        else
            chmod o-rx $path
        fi
        path="$(dirname $path)"
    done
}

sync_with_other_user()
{
    local skip_option=$1
    local path="$(dirname $__rbh_sync)"
    set_permission $path "+"

    local path_config="$(realpath $RBH_CONFIG_PATH)"
    set_permission $path_config "+"

    if [[ "$WITH_MPI" == "true" ]]; then
        # We need to give execute permissions to the user for mpirun to run
        chmod o+x ..
        local output="$(sudo -H -u "$test_user" bash -c \
                        "source /etc/profile.d/modules.sh; \
                         module load mpi/openmpi-x86_64; \
                         LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
                         RBH_CONFIG_PATH=$RBH_CONFIG_PATH \
                         mpirun $__rbh_sync --config $path_config $skip_option \
                         rbh:posix-mpi:. rbh:$db:$testdb" 2>&1)"
    else
        local output="$(sudo -E -H -u "$test_user" bash -c "\
                        LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
                        $__rbh_sync --config $path_config $skip_option \
                        rbh:posix:. rbh:$db:$testdb" 2>&1)"
    fi

    set_permission $path "-"

    set_permission $path_config "-"

    echo "$output"
}
