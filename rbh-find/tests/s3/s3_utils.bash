#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

minio_setup()
{
    export MC_INSECURE=1
    yes | mc alias set local https://127.0.0.1:9000 minioadmin minioadmin
}

minio_teardown()
{
    mc rb --force --dangerous local
}
