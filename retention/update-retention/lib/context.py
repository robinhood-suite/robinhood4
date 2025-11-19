#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import subprocess

from datetime import datetime, timedelta
from lib.utils import exec_check_output

class Context():
    """General context of the command"""
    def __init__(self, uri, config, delay, delete):
        self.uri = uri
        self.config = config
        self.delete = delete
        self.found_expired_dir = False

        dt = datetime.now()
        self.current = datetime.timestamp(dt)

        if delay == 0:
            self.delay = int(self.current)
        else:
            delay2days = timedelta(days=delay)
            self.delay = int(datetime.timestamp(dt + delay2days))

        self.mountpoint = exec_check_output(f"rbh-info {uri} -m")
