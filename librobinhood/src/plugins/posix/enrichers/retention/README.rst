.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

###################
Retention extension
###################

The Retention extension is an enricher for the POSIX plugin.
It is comprised of two parts: the information enrichment and the information
filtering and printing.

The first part is mainly used by rbh-sync_ and rbh-fsevents_.
The second part is used by rbh-find_.

.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Enrichment
==========

Here are the additional information enrichment for the POSIX plugin:
 * For all entries:
   * Expiration date if the entry has the retention attribute set

Filtering
=========

The Retention extension defines predicates and directives that will be used with
rbh-find_. The predicates define filters that will be usable by users to filter
data retrieved from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Below are defined the predicates defined by the Retention extension.

-expired-at
-----------

A predicate that filters entries which expired or will expire at a given epoch.
The expiration date is defined by the extended attribute defined in the
configuration file, or 'user.expires' by default, and can either be absolute or
relative to the mtime.

The predicate should be given an epoch, which can be prepended by a '+' or '-',
and the following is applied:
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

The predicate can also be given `inf`, in which case it will show all entries
that have an infinite expiration date set.

-expired
--------

A predicate which takes no argument and shows all files that are expired at the
the time of the command. Simply put, it behaves exactly ``-expired-at $(date
+%s)``.

.. code:: bash

    rbh-find rbh:mongo:test -expired
    ./dir/file-that-just-expired
    ./dir/file-that-expired-1-hour-ago

Printing
========

The Retention extension defines directives that will be used with rbh-find_.
They correspond to which data is printed when using the `-printf` and
`-fprintf` actions of rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Below are defined the directives allowed by the Retention extension.

+-------------+--------------------------------------------+
|  Directive  | Information printed                        |
+=============+============================================+
|     '%E'    | The expiration date of the entry           |
+-------------+--------------------------------------------+
|     '%e'    | The retention attribute as set by the user |
+-------------+--------------------------------------------+
