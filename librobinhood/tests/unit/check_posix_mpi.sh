#!/usr/bin/env bash

source /etc/profile.d/modules.sh
module load mpi/openmpi-x86_64

mpirun --allow-run-as-root "$1"/check_posix_mpi
