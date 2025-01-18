#!/usr/bin/env bash

module load mpi/openmpi-x86_64

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"

mpirun --allow-run-as-root "$1"/check_posix_mpi
