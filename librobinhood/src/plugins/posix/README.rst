.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

############
POSIX plugin
############

The POSIX plugin is a plugin that will retrieve POSIX specific information.
It mainly iterates through a POSIX hierarchy, checking each entry and enriching
a new FSEntry with the entry's metadata.

The plugin also provides predicates and directives to both filter found
FSEntries, and print specific information about them.

The iteration/enrichment part is mainly used by rbh-sync_ and rbh-fsevents_.
The predicates/directives part is mainly used by rbh-find_.

.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Enrichment
==========

Here are the additional information enrichment for the POSIX plugin:
 * For all entries:
   * statx information (see 'statx(2)' for more information):
     * access time, creation time, change time, modify time:
       * seconds
       * nanoseconds
     * block size
     * number of blocks
     * device and rdevices numbers:
       * major
       * minor
     * group ID
     * user ID
     * inode number
     * mode and type
     * number of links
     * size
   * name
   * path from the enrichment's mount point

Filtering
=========

The POSIX plugin defines predicates that will be used with rbh-find_. The
predicates define filters that will be usable by users to filter data retrieved
from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

However, since rbh-find_ is a close twin to `find(1)`__, we will only list
below the differences between its predicates, and rbh-find_'s ones.

.. __: find_
.. _find: http://man7.org/linux/man-pages/man1/find.1.html

-amin, -cmin, and -mmin
-----------------------

find's ``-[acm]min`` predicates do not work quite like ``-[acm]time`` in terms
of how the time boundaries are computed. There is no apparent reason for this.

rbh-find uses the same method for all 6 predicates which it borrows from find's
``-[acm]time``.

-size
-----

rbh-find's ``-size`` predicate works exactly like find's ``-size``, but with
the addition of the ``T`` size, for Terabytes.

-perm
-----

The implementation is still a work in progress as some differences with GNU find
still exist.

rbh-find's ``-perm`` predicate works like GNU find's except that GNU find
supports '-', '/' and '+' as a prefix for the mode string. The '+' is deprecated
and not used by GNU find but does not trigger a parsing error. Whereas, it is
a parsing error to use '+' in rbh-find as a prefix. Keep in mind that some
symbolic modes start with a '+' such as '+t' which corresponds to the sticky
bit. This '+' sign represents the operation to perform as '-' and '=' not the
prefix and is the reason for the deprecation of '+' as a prefix.

So looking for all the files with a sticky bit could be done with ``/+t``. And
``+t`` would match on file with only the sticky bit set and no other permission.

-blocks
-------

A predicate that will filter entries based on their number of blocks.

.. code:: bash

    rbh-find rbh:mongo:test -blocks 10
    /file-with-10-blocks-allocated

    rbh-find rbh:mongo:test -blocks -42
    /empty-file
    /file-with-10-blocks-allocated
    /file-with-30-blocks-allocated


Printing
========

The POSIX plugin defines directives that will be used with rbh-find_.
They correspond to which data is printed when using the `-printf` and
`-fprintf` actions of rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Similarly to the directives, most of the POSIX directives are the same ones
provided by `find(1)`__, so we will only list the differences.

.. __: find_
.. _find: http://man7.org/linux/man-pages/man1/find.1.html

+-------------+--------------------------------------------+
|  Directive  | Information printed                        |
+=============+============================================+
|     '%I'    | The ID of the entry computed by RobinHood  |
+-------------+--------------------------------------------+
