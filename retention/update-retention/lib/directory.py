#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import os
import subprocess

from datetime import datetime
from lib.utils import exec_popen, exec_run

def _set_max_time(directory, line):
    if int(line) > directory.max_time:
        directory.max_time = int(line)

class Directory():
    """Class representing a directory as output of rbh-find"""
    def __init__(self, path, retention_attr, expiration_date, ID):
        self.path = path
        self.relative_retention = (retention_attr[0] is '+')
        self.retention_attr = (retention_attr[1:] if self.relative_retention
                                                  else retention_attr)
        dt = datetime.strptime(expiration_date, "%a %b %d %H:%M:%S %Y")
        self.expiration_date = dt.timestamp()
        self.ID = ID

    def set_max_time(self, uri):
        branch = f"{uri}#{self.path}"
        command = f"rbh-find {branch} -type f -printf %A\\n%T\\n"
        self.max_time = 0
        exec_popen(command, _set_max_time, self)

    def actual_expiration_date(self):
        return int(self.retention_attr) + self.max_time

    def is_truly_expired(self, current):
        return self.actual_expiration_date() <= current

    def is_empty(self):
        return self.max_time == 0

    def update_expiration_date(self, context, verbose=False):
        new_expiration_date = int(self.actual_expiration_date() + 1)

        if verbose:
            print(f"Expiration of the directory should occur "
                  f"'{self.retention_attr}' seconds after it's last usage.")
            print(f"Changing the expiration date of '{self.path}' to "
                  f"'{datetime.fromtimestamp(new_expiration_date)}'")

        command = f"rbh-fsevents - {context.uri}"
        string = (f"--- !inode_xattr\n"
                  f"\"id\": !!binary {self.ID}\n"
                  f"\"xattrs\":\n"
                  f"  \"trusted.expiration_date\": !int64 {new_expiration_date}\n"
                  f"...\n")

        exec_run(command, string)
        os.setxattr(f"{context.mountpoint}/{self.path}",
                     "trusted.expiration_date",
                     (str(new_expiration_date)).encode("utf-8"))
