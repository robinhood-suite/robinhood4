#!/usr/bin/env python3

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

import argparse
import pathlib
import re
import shutil
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

def handle_non_relative_expiration(context, directory):
    print(f"Directory '{directory.path}' expired on "
          f"'{datetime.fromtimestamp(directory.expiration_date)}'")

    if context.delete:
        shutil.rmtree(f"{context.mountpoint}/{directory.path}",
                      ignore_errors=True)

def handle_truly_expired_empty_directory(context, directory):
    print(f"Directory '{directory.path}' has expired and is empty, no other "
           "check needed")

    if context.delete:
        command = (["find", f"{context.mountpoint}/{directory.path}",
                    "-depth", "-exec", "\"rmdir\"", "{}", "\";\""])

    try:
        process = subprocess.check_output(command)

    except subprocess.CalledProcessError as e:
        print(f"find failed: {e.output.decode('utf-8')}")
        sys.tracebacklimit = -1
        return 1

def handle_truly_expired_directory(context, directory):
    print(f"The last accessed file in it was accessed on "
          f"'{datetime.fromtimestamp(directory.max_time)}'")
    print(f"Expiration of the directory should occur "
          f"'{directory.retention_attr}' seconds after it's last usage")

    if context.delete:
        print(f"Directory '{directory.path}' has expired and will be deleted")
        shutil.rmtree(f"{context.mountpoint}/{directory.path}",
                      ignore_errors=True)
    else:
        print(f"Directory '{directory.path}' has expired")

def check_directory_expirancy(context, _dir_info):
    dir_info = _dir_info.split("|")

    if not dir_info[0]:
        print(f"Directory in '{_dir_info}' doesn't have a proper path, "
              "skipping it.")
        return
    elif dir_info[1] == "None":
        print("Expiration attribute is unknown, so we cannot determine if the "
              f"directory is really expired or not. Skipping '{dir_info[0]}'")
        return

    directory = Directory(path = dir_info[0], retention_attr = dir_info[1],
                          expiration_date = dir_info[2], ID = dir_info[3])

    if not directory.relative_retention:
        handle_non_relative_expiration(context, directory)
        return

    directory.set_max_time(context.uri)

    if directory.is_truly_expired(context.current):
        print(f"Directory '{directory.path}' expiration date is set to "
              f"'{datetime.fromtimestamp(directory.expiration_date)}'")
        if directory.is_empty():
            handle_truly_expired_empty_directory(context, directory)
        else:
            handle_truly_expired_directory(context, directory)
        return

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
