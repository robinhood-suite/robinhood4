.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

####################
rbh-update-retention
####################

rbh-update-retention_ is a script that will go through each expired directory
in the given URI, check if they are `truly expired` and delete them if so.

Retention
=========

This script relies on retention attributes set by users on their files and
directories as extended attributes. The standard xattr considered is
``user.expires`` but that attribute can be changed with the configuration file
(see the Lustre backend's documentation__ for more information).

__ https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#lustre-backend

When a file or directory holding such an attribute is synchronized into a mirror
system, the actual expiration date is computed. That file or directory is then
considered `expired` when that date is passed.

Inner working
=============

rbh-update-retention_'s goal is to iterate through every directory considered
expired in a given URI, and for each one, get the most recent access or modify
time for files in the directory's hierarchy. Then, it will compute the sum
between the former and the expired directory's expiration attribute.

Finally, if the result is later than the current date, rbh-update-retention_
will update the directory's expiration date to that result. If the result isn't,
that directory is considered `truly expired` and will be printed out and may be
deleted.

Usage
=====

rbh-update-retention_ must be given two parameters: an URI__ in which to search
expired directories and their file hierarchy, and a mount point to either
update or delete the (non-)truly expired directories.

.. __URI: https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#uri

The script can also be provided a `--delete` flag, which will indicate what to
do with the truly expired directories. By default, if not provided, they will
simply be printed out. If provided, they will be deleted.

It can also be provided a path to a configuration file with the `--config`
option, which will be used to determine the retention attribute to check for by
tools used in rbh-update-retention_.

Examples
--------

The following examples all assume you have a backend set up at
``rbh:mongo:test``. [#]_

.. code:: bash

    # Create a directory and file, set that directory's expiration to 5 seconds
    # after its last modification, sync the hierarchy
    $ mkdir dir
    $ touch dir/file
    $ setfattr -n user.expires -v +5 dir
    $ rbh-sync rbh:lustre:. rbh:mongo:testdb

    # Wait 10 seconds, then check if that directory is truly expired
    $ sleep 10
    $ rbh-update-retention rbh:mongo:testdb .
    Directory 'dir' expiration date is set to '{current time - 5 seconds}'
    The last accessed file in it was accessed on '{current time - 10 seconds}'
    Directory 'dir' has expired

    # If we then touch the file in dir again, sync them again, and check if
    # that directory is truly expired
    $ touch dir/file
    $ rbh-sync rbh:lustre:. rbh:mongo:testdb
    $ rbh-update-retention rbh:mongo:testdb .
    Directory 'dir' expiration date is set to '{current time - 5 seconds}'
    The last accessed file in it was accessed on '{current time - 10 seconds}'
    Expiration of the directory should occur '5' seconds after it's last usage
    Changing the expiration date of 'dir' to '{current time + 5 seconds}'

    # Finally, if we wait 10 seconds again, and try to delete truly expired
    # directories
    $ sleep 10
    $ rbh-update-retention rbh:mongo:testdb . --delete
    Directory 'dir' expiration date is set to '{current time - 5 seconds}'
    The last accessed file in it was accessed on '{current time - 10 seconds}'
    Directory 'dir' has expired and will be deleted


.. [#] to set up a backend, have a look at rbh-sync_'s documentation
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
