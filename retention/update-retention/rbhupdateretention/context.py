#!/usr/bin/env python3

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import os
import subprocess

from datetime import datetime, timedelta
from rbhupdateretention.utils import exec_check_output

class Context():
    """General context of the command"""
    def __init__(self, uri, config, delay, delete, delete_parent_if_empty):
        self.uri = uri
        self.delete = delete
        self.delete_parent_if_empty = delete_parent_if_empty
        self.found_expired_dir = False

        if self.delete_parent_if_empty and not self.delete:
            print('\'--delete-parent-if-empty\' specified but not '
                  '\'--delete\', will not take effect.')

        dt = datetime.now()
        self.current = datetime.timestamp(dt)

        if delay == 0:
            self.delay = int(self.current)
        else:
            delay2days = timedelta(days=delay)
            self.delay = int(datetime.timestamp(dt + delay2days))

        self.config = config
        if self.config is None:
            self.config = os.environ.get('RBH_CONFIG_PATH',
                                         '/etc/robinhood4.d/default.yaml')

        mnt = exec_check_output(f"rbh-info {uri} -m")
        self.mountpoint = mnt[mnt.find('/'):]
