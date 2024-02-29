#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if which module ; then
    exit 1
fi

module load mpi/openmpi-x86_64
if [[ $? -eq 1 ]]; then
    exit 1
fi
if ! which mpirun ; then
    exit 1
fi

exit 0
