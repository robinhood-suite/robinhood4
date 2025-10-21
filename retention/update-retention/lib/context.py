#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import subprocess

from datetime import datetime, timedelta

class Context():
    """General context of the command"""
    def __init__(self, uri, config, delay, delete):
        self.uri = uri
        self.config = config
        self.delete = delete

        dt = datetime.now()
        self.current = datetime.timestamp(dt)

        if delay is 0:
            self.delay = int(self.current)
        else:
            two_days = timedelta(days=args.delay)
            self.delay = int(datetime.timestamp(dt + two_days))

        command = (["rbh-info", uri, "-m"])

        try:
            process = subprocess.check_output(command)
            self.mountpoint = process.decode('utf-8').rstrip()

        except subprocess.CalledProcessError as e:
            print(f"rbh-info failed: {e.output.decode('utf-8')}")
            sys.tracebacklimit = -1

