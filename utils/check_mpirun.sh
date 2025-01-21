#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

source /etc/profile.d/modules.sh

if ! type -t module ; then
    echo "'module' not found"
    exit 1
fi

module load mpi/openmpi-x86_64
if [[ $? -eq 1 ]]; then
    echo "failed to load openmpi module"
    exit 1
fi

if ! which mpirun ; then
    echo "'mpirun' not found"
    exit 1
fi

exit 0
