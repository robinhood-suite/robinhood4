#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import subprocess

from datetime import datetime

class Directory():
    """Class representing a directory as output of rbh-find"""
    def __init__(self, path, retention_attr, expiration_date, ID):
        self.path = path
        self.relative_retention = (retention_attr[0] == '+')
        self.retention_attr = (retention_attr[1:] if self.relative_retention
                                                  else retention_attr)
        dt = datetime.strptime(expiration_date, "%a %b %d %H:%M:%S %Y")
        self.expiration_date = dt.timestamp()
        self.ID = ID

    def set_max_time(self, uri):
        branch = f"{uri}#{self.path}"
        command = (["rbh-find", branch, "-type", "f",
                    "-printf", "%A\n%T\n"])

        try:
            process = subprocess.Popen(command, stdout=subprocess.PIPE)
        except subprocess.CalledProcessError as e:
            print(f"rbh-find failed: {e.output.decode('utf-8')}")
            sys.tracebacklimit = -1
            return 1

        print(f"args = '{process.args}'")

        self.max_time = 0
        for line in iter(process.stdout.readline, b""):
            line = line.decode('utf-8').rstrip()
            if int(line) > self.max_time:
                self.max_time = int(line)

    def actual_expiration_date(self):
        return int(self.retention_attr) + self.max_time

    def is_truly_expired(self, current):
        return self.actual_expiration_date() <= current

    def is_empty(self):
        return self.max_time == 0
>>>>>>> 64c11fcb (update-ret: properly define directory class and functions)
