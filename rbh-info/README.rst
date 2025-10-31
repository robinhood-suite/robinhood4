.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

########
rbh-info
########

rbh-info displays general information about RobinHood plugins and backends.

Overview
========

rbh-info reports information about installed RobinHood plugins, each plugin's
capabilities, and general metadata about a specific `RobinHood backend`_.

.. _RobinHood Backend:
       https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#backend

Usage
=====

Basic usage examples:

::

    # List installed plugins
    rbh-info --list

    # Show capabilities of a plugin
    rbh-info posix

    # Show general information about a backend
    rbh-info rbh:mongo:test

Where does rbh-info search installed RobinHood plugins
======================================================

rbh-info searches for installed plugins in the same way `dlopen(3)`__ does when
loading dynamic shared objects. It looks in common library locations such as
`/lib`, `/usr/lib`, `/lib64`, and `/usr/lib64`, and also checks the directories
listed in the `LD_LIBRARY_PATH` environment variable.

.. __: dlopen_
.. _dlopen: https://man7.org/linux/man-pages/man3/dlopen.3.html

Capabilities
============

rbh-info can display the capabilities of a specific plugin. The capabilities
of a plugin indicate which tools or operations the plugin supports and whether
it can act as a source and/or a destination backend.

The available capabilities are:

* filter: the plugin supports reading data with filters
* synchronization: the plugin can be used to read data
* update: the plugin supports updating metadata or entries in the backend
* branch: the plugin can operate on a subsection of the backend

General backend information
===========================

When pointed at a Robinhood backend, rbh-info can report various information and
metadata.

All the available information are:

* average object size: average size of entries stored in the backend
* backend source: backend used to create the backend
* count: total number of entries in the backend
* first sync: metadata about the first synchronization that created the backend
* last sync: metadata about the most recent synchronization
* size: total storage size used by the backend

The synchronization metadata are:

* start date/time
* end date/time
* duration
* mountpoint used during the sync
* CLI command used to perform the sync
* number of entries synchronized
* number of entries skipped
* number of entries seen

.. code:: bash

    # Get the total number of entries in a backend
    rbh-info rbh:mongo:test --count

    # Show metadata about the first synchronisation
    rbh-info rbh:mongo:test --first-sync
