#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import argparse
import pathlib
import re
import subprocess
import sys

from lib.context import Context
from lib.directory import Directory

def looks_like_URI(string):
    pattern = re.compile(r"rbh:mongo:[A-Za-z0-9]+", re.IGNORECASE)
    if pattern.match(string) is None:
        raise TypeError(f"'{string}' does not look like a URI, expected "
                         "'rbh:mongo:<db_name>'")

    return string

def make_parser():
    """Create command line parser"""

    parser = argparse.ArgumentParser(
        description='Update the retention of attributes of any directory that '
                    'is expired, based on their content\'s last accessed time. '
                    'If a directory is expired, it is printed to the output.'
    )

    parser.add_argument('uri', metavar='URI', type=looks_like_URI,
                        help='URI of where to fetch and update expired '
                             'directories')
    parser.add_argument('-c', '--config', metavar='CONFIG', type=pathlib.Path,
                        default='/etc/robinhood4.d/default.yaml',
                        help='path to a configuration file, used to determine '
                             'the expiration attribute to check for. By '
                             'default, use \'/etc/robinhood4.d/default.yaml\'')
    parser.add_argument('-d', '--delay', metavar='DELAY', default=0, type=int,
                        help='a delay window during which entries that will '
                             'expire should be notified. Should be given as '
                             'a number of days. No delay by default.')
    parser.add_argument('--delete', action='store_true', default=False,
                        help='delete the expired directories instead of just '
                             'printing. False by default.')

    return parser

def check_directory_expirancy(context, _dir_info):
    dir_info = _dir_info.split("|")
    directory = Directory(path = dir_info[0], retention_attr = dir_info[1],
                          expiration_date = dir_info[2], ID = dir_info[3])
    directory.set_max_time(context.uri)

def main(args=None):
    args = make_parser().parse_args(args)

    context = Context(args.uri, args.config, args.delay, args.delete)
    command = (["rbh-find", "-c", str(context.config), context.uri,
                "-type", "d", "-expired-at", str(context.delay),
                "-printf", "%p|%e|%E|%I\n"])

    try:
        process = subprocess.run(command, stdout=subprocess.PIPE, check=True)
        for line in iter(process.stdout.readline, b""):
            line = line.decode('utf-8').rstrip()
            check_directory_expirancy(context, line)

    except subprocess.CalledProcessError as e:
        print(f"rbh-find failed: {e.output.decode('utf-8')}")
        sys.tracebacklimit = -1
        return 1

    return 0

if __name__ == "__main__":
    main()
