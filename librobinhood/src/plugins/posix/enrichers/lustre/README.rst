.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

###############
rbh-find-lustre
###############

rbh-find-lustre is an overload of `rbh-find(1)`__.

.. __: rbh-find_
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

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

    rbh-lfind rbh:mongo:test -stripe-count 1
    /dir/file-with-stripe-count-1

    lfs setstripe --stripe-count 2 /mnt/lustre
    rbh-lfind rbh:mongo:test -stripe-count 2
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
``default``, ``raid0``, ``mdt``, ``released`` and ``overstriped``. It behaves
exactly like the ``stripe-count`` predicate with regards to the default value
otherwise.

-expired-at
-----------

rbh-lfind defines a ``-expired-at`` predicate that filters entries which
expired or will expire at a given epoch. The expiration date is defined by the
extended attribute defined in the configuration file, or 'user.expires' by
default, and can either be absolute or relative to the maximum between the
atime, ctime and mtime.

The predicate can be given an epoch, which can be prepended by a '+' or '-', and
the following is applied:
 - <n>: match files that expired at and before epoch <n>
 - -<n>: match files that expired before epoch <n>
 - +<n>: match files that expire after epoch <n>

.. code:: bash

    rbh-lfind rbh:mongo:test -expired-at -$(date +%s)
    ./dir/file-that-expired-1-hour-ago

    rbh-lfind rbh:mongo:test -expired-at $(date +%s) -o \
        -expired-at +$(date +%s -d "5 minutes")
    ./dir/file-that-just-expired
    ./dir/file-that-expired-1-hour-ago
    ./dir/file-that-will-expire-in-10-minutes
    ./dir/file-that-will-expire-in-2-days

    rbh-lfind rbh:mongo:test -expired-at +$(date +%s) -o \
        -expired-at -$(date +%s -d "1 day")
    ./dir/file-that-will-expire-in-10-minutes

The predicate can also be given `inf`, in which case it will show all entries
that have an infinite expiration date set.

-expired
--------

rbh-lfind defines a ``-expired`` predicate which takes no argument and shows
all files that expired at and before the time of the command. Simply put, it
behaves exactly ``-expired-at $(date +%s)``.

.. code:: bash

    rbh-lfind rbh:mongo:test -expired
    ./dir/file-that-just-expired
    ./dir/file-that-expired-1-hour-ago

-comp-start
-----------

rbh-lfind defines a ``-comp-start`` predicate that filters entries based on
the component start (or `begin`) of any of their components. It follows the
logic of a size filter, and thus will filters entries following this logic:

.. code:: bash

    rbh-lfind rbh:mongo:test -comp-start 5M # -> filter entries that have a
    # component start within the interval ]4M ; 5M]

    rbh-lfind rbh:mongo:test -comp-start +10M # -> filter entries that have a
    # component start within the interval ]10M ; +oo[

    rbh-lfind rbh:mongo:test -comp-start -10M # -> filter entries that have a
    # component start within the interval [0 ; 9M]

Be wary that filtering entries using this predicate twice will NOT filter
entries that a component start that matches the first predicate AND the second
predicate at the same time. Therefore, ``-comp-start +10M -comp-start -20M``
will not give entries that a component start both superior to 10M and inferior
to 20M. Rather, it will output entries with that have a component start
superior to 10M and a component start inferior to 20M, whether they are the
same component start or not.

To allow the above expected behaviour, we allow the input field of the
``comp-start`` predicate to be two values separated by a comma. In that case
the first half is expected to be the lower bound of the interval while the
second is the high bound (following the exact filtering behaviour).

Thus, ``comp-start 10M,20M`` will filter entries that have a component start
superior to 9M and inferior or equal to 20M.

To showcase the difference:

.. code:: bash

    lfs setstripe -E 1G -S 256k -E -1 -S 512k "comp_start_at_0_and_1G"

    rbh-lfind rbh:mongo:test -comp-start +1M -comp-start -2M
    ./comp_start_at_0_and_1G # -> this is because the component start at 1G
    # satisfies the first predicate and the component start at 0 satisfies the
    # second

    rbh-lfind rbh:mongo:test -comp-start 1M,2M
    # nothing is outputted because `comp_start_at_0_and_1G` doesn't have a
    # component start in the interval ]0 ; 2M]

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
