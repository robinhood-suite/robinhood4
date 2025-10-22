#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import shutil
import subprocess
import sys

def exec_check_output(_command):
    command = _command.split()
    try:
        process = subprocess.check_output(command)
        return process.decode('utf-8').rstrip()

    except subprocess.CalledProcessError as e:
        print(f"{command} failed: {e.output.decode('utf-8')}")
        sys.tracebacklimit = -1
        sys.exit(e.returncode)

def exec_popen(_command, output_callback, context):
    command = _command.split()
    try:
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   bufsize=1)
        for line in iter(process.stdout.readline, b""):
            line = line.decode('utf-8').rstrip()
            output_callback(context, line)

    except subprocess.CalledProcessError as e:
        print(f"{command} failed: {e.output.decode('utf-8')}")
        sys.tracebacklimit = -1
        sys.exit(e.returncode)

def rm_tree(tree):
    shutil.rmtree(tree, ignore_errors=True)
