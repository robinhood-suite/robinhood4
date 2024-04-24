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
    /file
    /dir/file-0
    /dir/file-1

    rbh-find rbh:mongo:test -hsm-state archived -hsm-state norelease
    /dir/file-0

-fid
----

rbh-lfind defines a ``-fid`` predicate that filters the entries on an exact fid.

.. code:: bash

    rbh-find rbh:mongo:test -fid [0xa:0xb:0xc]
    /dir/file-1

-ost
----

rbh-lfind defines a ``-ost`` predicate that currently filters the entries on an
exact OST index.

.. code:: bash

    rbh-find rbh:mongo:test -ost 1 -o ost 2
    /dir/file-on-ost-1
    /dir/file-on-ost-2
    /dir/file-on-ost-1-and-2

-stripe-count
-------------

rbh-lfind defines a ``-stripe-count`` predicate that filters entries based on
the stripe count of any of their components. It supports filtering on a stripe
count equal, larger or lower than ``n``, by respectively using ``n``, ``+n`` or
``-n``.

Moreover, the value given can be ``default``, in which case it will filter all
that do not have a stripe count set. This will only match directories however,
because only directories have a default striping, as it does not make sense for
other types of entries in Lustre.

Finally, if given a numerical value, rbh-lfind will fetch the default stripe
count of the current mount point (meaning we assume rbh-lfind's working
directory is under the right filesystem mount point), and compare it to the
value that is to be filtered. If they match (i.e. if the default stripe is
equal, larger or lower then the value given by the user), every directory with a
default stripe count will be printed.

.. code:: bash

    rbh-find rbh:mongo:test -stripe-count 1
    /dir/file-with-stripe-count-1

    lfs setstripe --stripe-count 2 /mnt/lustre
    rbh-find rbh:mongo:test -stripe-count 2
    /dir/file-with-stripe-count-2
    /directory-with-default-striping
    /

-stripe-size
------------

rbh-lfind defines a ``-stripe-size`` predicate that filters entries based on
the stripe size of any of their components. It behaves exactly like the
``stripe-count`` predicate otherwise.

-layout-pattern
---------------

rbh-lfind defines a ``-layout-pattern`` predicate that filters entries based on
the pattern of any of their components. The only accepted values are
``default``, ``raid0`` and ``mdt``. It behaves exactly like the
``stripe-count`` predicate otherwise.

Examples
--------

WORK IN PROGRESS
