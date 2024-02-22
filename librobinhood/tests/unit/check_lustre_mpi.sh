#!/usr/bin/env bash

module load mpi/openmpi-x86_64

mpirun --allow-run-as-root -np 4 "$@"/check_lustre_mpi_missing

mpirun --allow-run-as-root -np 4 "$@"/check_lustre_mpi_empty
