#!/usr/bin/env bash

module load mpi/openmpi-x86_64

mpirun --allow-run-as-root -np 4 ../build/tests/unit/check_lustre_mpi
