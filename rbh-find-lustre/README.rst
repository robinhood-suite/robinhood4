.. This file is part of RobinHood 4
   Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

###############
rbh-find-lustre
###############

rbh-find-lustre is an overload of `rbh-find(1)`__.

.. __: rbh-find_
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Installation
============

Install the `RobinHood library`_ and the `rbh-find library`_ first, then
download the sources:

Build and install with meson_ and ninja_:

.. code:: bash

    cd robinhood4/rbh-find-lustre
    meson builddir
    ninja -C builddir
    sudo ninja -C builddir install

.. _meson: https://mesonbuild.com
.. _ninja: https://ninja-build.org
.. _RobinHood library: https://github.com/robinhood-suite/robinhood4/tree/main/librobinhood
.. _rbh-find library: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

A work in progress
==================

rbh-find-lustre is a close twin of `rbh-find(1)`__. It focuses more on the use
of the Lustre backend of the `RobinHood library`_. As such, it will implement
some Lustre specific features, but will still behave exactly like `rbh-find`.

.. __: _rbh-find

Usage
=====

`rbh-lfind` works so much like `rbh-find` that documenting how it works would
look a lot like `rbh-find's documentation`_. Instead, we will document the
differences of `rbh-lfind` compared to `rbh-find`.

.. _: https://github.com/robinhood-suite/robinhood4/blob/main/rbh-find/README.rst

Extra features
==============

-hsm-state
----------

rbh-lfind defines a ``-hsm-state`` predicate that filters the entries on their
HSM status. These include: archived, dirty, exists, lost, noarchive, norelease,
none and released.

.. code:: bash

    rbh-find rbh:mongo:test -hsm-state archived
    ./file
    ./dir/file-0
    ./dir/file-1

    rbh-find rbh:mongo:test -hsm-state archived -hsm-state norelease
    ./dir/file-0

-fid
----

rbh-lfind defines a ``-fid`` predicate that filters the entries on an exact fid.

.. code:: bash

    rbh-find rbh:mongo:test -fid [0xa:0xb:0xc]
    ./dir/file-1

-ost
----

rbh-lfind defines a ``-ost`` predicate that currently filters the entries on an
exact OST index.

.. code:: bash

    rbh-find rbh:mongo:test -ost 1 -o ost 2
    ./dir/file-on-ost-1
    ./dir/file-on-ost-2
    ./dir/file-on-ost-1-and-2

-expired-at
-----------

rbh-lfind defines a ``-expired-at`` predicate that filters entries which
expired or will expire at a given epoch. The expiration date is defined
by the extended attribute 'user.ccc_expires', and can either be absolute
or relative to the maximum between the atime, ctime and mtime.

The predicate can be given an epoch, which can be prepended by a '+' or '-', and
the following is applied:
 - <n>: match files that expired at and before epoch <n>
 - -<n>: match files that expired before epoch <n>
 - +<n>: match files that expire after epoch <n>

.. code:: bash

    rbh-find rbh:mongo:test -expired-at -$(date +%s)
    ./dir/file-that-expired-1-hour-ago

    rbh-find rbh:mongo:test -expired-at $(date +%s) -o \
        -expired-at +$(date +%s -d "5 minutes")
    ./dir/file-that-just-expired
    ./dir/file-that-expired-1-hour-ago
    ./dir/file-that-will-expire-in-10-minutes
    ./dir/file-that-will-expire-in-2-days

    rbh-find rbh:mongo:test -expired-at +$(date +%s) -o \
        -expired-at -$(date +%s -d "1 day")
    ./dir/file-that-will-expire-in-10-minutes

-expired
--------

rbh-lfind defines a ``-expired`` predicate which takes no argument and shows
all files that expired at and before the time of the command. Simply put, it
behaves exactly ``-expired-at $(date +%s)``.

.. code:: bash

    rbh-find rbh:mongo:test -expired
    ./dir/file-that-just-expired
    ./dir/file-that-expired-1-hour-ago

-printf
-------

rbh-lfind defines a ``printf`` action that will print information specific to
Lustre. More specifically, we define the following directives:
 - '%E': to print the expiration date of the entry
 - '%e': to print the expiration attribute as set by the user
 - '%I': to print the ID of an entry in base64

Examples
--------

WORK IN PROGRESS
