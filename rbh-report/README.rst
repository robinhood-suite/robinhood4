.. This file is part of Robinhood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

##########
rbh-report
##########

rbh-report allows obtaining aggregated information on a backend.

Overview
========

rbh-report can be used to create `report` from a `RobinHood Backend`_ [#]_.
These reports are created by aggregating information in the requested backend.

.. _RobinHood Backend:
       https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#backend

Usage
=====

The basic syntax for rbh-report is:

::

    rbh-report SOURCE --output OUTPUT

The SOURCE must be a RobinHood URI. The ``--output`` option is mandatory and
specifies the aggregation to compute.

.. code:: bash

    rbh-report rbh:mongo:test --output "max(statx.size)"

Output
======

The ``--output`` option defines which statistics to compute. Each expression
has the form:

::

    <accumulator>(<field>)

Multiple expressions can be specified in CSV format.

Fields
------

All the fields available for rbh-report are:

::

    attributes atime.nsec atime.sec blksize
    blocks     btime.nsec btime.sec
    ctime.nsec ctime.sec  dev.major dev.minor
    gid        ino        mode
    mtime.nsec mtime.sec  nlink
    rdev.major rdev.minor size
    type       uid


You must use the prefix `statx` when using them with an accumulator or a
group-by clauses.

Accumulators
------------

All the accumulator available for rbh-report are:

* avg(field): average value of the field
* max(field): maximum value of the field
* min(field): minimum value of the field
* sum(field): sum of the field values

The special accumulator `count()` takes no field and returns the number of
matched entries.

* count(): number of entries

Example:

.. code:: bash

    rbh-report rbh:mongo:test --output "max(statx.size),avg(statx.size),count()"

     max(statx.size) | avg(statx.size) | count()
    ---------------------------------------------
            1024     |             442 |       4

Group-by
========

You can group resuts by one or more fields using the ``--group-by`` option.
The ``--group-by`` option can takes as input a CSV to group on multiple field.
The report will contain one row per grouping.

Example:

.. code:: bash

    # Get for each type the maximum size
    rbh-report rbh:mongo:test --group-by statx.type --output "max(statx.size)"

     statx.type || max(statx.size)
    -------------------------------
      directory ||              45
           file ||            1024

Range
-----

A group-by field may be used with a numeric range to create a subgroup on that
field. To specify a range, you must add an array after the field. Each array's
element must be separated with semicolon (;).

::

    statx.size[0;500;1000]

.. code:: bash

    # Get the number of entry between 0 and 500, 500 and 1000 and more than 1000
    rbh-report rbh:mongo:test --group-by "statx.size[0;500;1000]" \
        --output "count()"

      statx.size || count()
    -------------------------
        [0; 500] ||       2
     [500; 1000] ||       1
    [1000; +inf] ||       1

Filtering
=========

rbh-report supports predicates the same way as rbh-find_. It can be used to
filter which entries are considered for aggregation.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

.. code:: bash

    rbh-report rbh:mongo:test --output "max(statx.size)" -type f

     max(statx.size)
    -----------------
            1024
