#!/usr/bin/env python3.9
# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import os
import tempfile
import shutil
import unittest
import sys

from importlib import import_module
from pathlib import Path

import subprocess

if __name__ == "__main__":
    os.chdir('../..')
    try:
        output = subprocess.check_output(["sh", "retention/update-retention/tests/test_update_retention.bash"],
                                         stderr=subprocess.STDOUT,
                                         universal_newlines=True)
    except subprocess.CalledProcessError as exc:
        print("Status : FAIL", exc.returncode, exc.output)
        sys.exit(exc.returncode)
    else:
        print("Output: \n{}\n".format(output))
