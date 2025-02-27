.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

##############################
Build-to-run retention (build)
##############################

This document describes how to install and setup the retention feature of
RobinHood 4.

Context
=======

The retention feature aims to add expiration date to files and directories. The
goal is to ensure unused files are automatically archived to more capacitive
storage systems or deleted altogether.

This feature is based on user provided extended attributes that specify an
entry's expiration date and that entry's access and/or modification times. For
more explanation about the attribute, check the `Retention Logic`__ section.

Those metadata and attributes are stored in a Mongo database, and queried using
the suite's tools.

Installation
============

Installation of the suite and tools is done on admin nodes with access to user
files.

.. code:: sh

    yum install robinhood4

Configuration
=============

The default configuration file is located at `/etc/robinhood4.d/rbh_conf.yaml`:

.. code:: yaml

    # The connection string to indicate to the mongo backend where and how to
    # connect to the MongoDB database. If not set, the mongo backend will use
    # "mongodb://localhost:27017".
    #
    # See https://www.mongodb.com/docs/manual/reference/connection-string/ for
    # more information about connection string.
    RBH_MONGO_DB_ADDRESS: "mongodb://localhost:27017"

    # Name of the xattr used to store the expiration date of a file or
    # directory.  It will be used by the retention. If not set, the retention
    # will search the "user.expires" xattr by default.
    RBH_RETENTION_XATTR: user.expires

    # Define the "retention" backend that can be called with rbh-sync to
    # retrieve retention information on top of POSIX information.
    backends:
        retention:
            extends: posix
            enrichers:
                - retention

Startup
=======

The database can be started with systemctl:

.. code:: sh

    systemctl start mongod

Tests
=====

To verify RobinHood 4 and the Mongo database run correctly, use the following
commands:

.. code:: sh

    rbh-sync rbh:posix:<any directory> rbh:mongo:test_db
    mongosh test_db --eval "db.entries.find()"

If the `mongosh` command outputs entries, that means the synchronization worked
properly, and the database/RobinHood 4 are correctly set up.

To specifically test the retention feature, do the following:

.. code:: sh

    # Create a random file and set its expiration date to 5 seconds in the future
    touch random_file.txt
    setfattr -n user.expires -v +5 random_file.txt

    # Synchronize the file to the `test_db` database using the retention backend
    # as defined in the configuration file
    rbh-sync rbh:retention:random_file.txt rbh:mongo:test_db

    # Sleep 5 seconds then check the database for expired files
    sleep 5
    rbh-lfind rbh:mongo:test_db -expired

The last command should show you the `random_file.txt` as expired. If it does,
the retention feature works properly.

Retention Logic
===============

There are two different logic for files and for directories. But overall, the
retention attribute is parsed the same and has the same meaning for both.

The retention attribute is set by users on their files and directories. By
default, it is the `user.expires` attribute, and its value will correspond to an
epoch, and can be preceded by a plus sign.

If the value is solely an epoch, it will be considered as the expiration date of
the entry, regardless of when it was last accessed.

If the value is preceded by a plus sign, the given epoch will be compared to
the maximum between the file's access time and modification time, or the
directory's modification time. This means that the entry will expire when
**max(atime, mtime) + epoch <= queried_epoch**. This calculation is only
performed when synchronizing, and not when querying expired entries.

This logic is applied as-is for files.

For directories, the logic is different:

::

    for every expired directory:
        last_time = most recent access or modify time of any file in the directory
        retention_attr = retention attribute set on the directory
        if last_time + retention_attr > current time:
            update the directory's expiration date to last_time + retention_attr
        else
            consider the directory truly expired

This logic is only applied to directories with a relative expiration date, and
not a set one. It is used to verify that a directory is expired by checking
a file in it hasn't been accessed or modified more recently. If there is one,
we delay the directory's expiration, otherwise we continue as normal.

If a directory has a set expiration date (i.e. the retention attribute is an
actual epoch), the same logic as files is applied.

Architecture
============

Mongo
-----

A MongoDB instance to hold the databases created by tools from RobinHood4.
To be installed on a single storage node, but may be shared accross multiple
nodes via sharding.

RobinHood4
----------

The suite of tools that will synchronize a filesystem's metadata to a Mongo
database, update, and allow retrieval of expired entries.
To be installed on a single admin node, but may be extended to multiple nodes
for usage with MPIFileUtils.
