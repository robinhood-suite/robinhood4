.. This file is part of rbh-find
   Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

########
rbh-find
########

rbh-find is a close twin of `find(1)`__.

.. __: find_
.. _find: http://man7.org/linux/man-pages/man1/find.1.html

Installation
============

Install the `RobinHood library`_ first, then download the sources:

.. code:: bash

    git clone https://github.com/cea-hpc/rbh-find.git
    cd rbh-find

Build and install with meson_ and ninja_:

.. code:: bash

    meson builddir
    ninja -C builddir
    sudo ninja -C builddir install

.. _meson: https://mesonbuild.com
.. _ninja: https://ninja-build.org
.. _RobinHood library: https://github.com/cea-hpc/librobinhood

A work in progress
==================

rbh-find is a close twin of `find(1)`__. At least it aims at being one. Right
now, it is more like the child version of find's twin.

The structure of the project is all done. The rest should be easy enough.
Feel free to contribute!

.. __: find_

Usage
=====

rbh-find works so much like find that documenting how it works would look a lot
like `find's man page`__. Instead, we will document the differences of rbh-find
compared to find. That and a few examples should be enough for users to figure
things out.

.. __: find_

Examples
--------

The following examples all assume you have a backend set up at
``rbh:mongo:test``. [#]_

.. code:: bash

    # find every file with a txt extension
    rbh-find rbh:mongo:test -type f -name '*.txt'

    # find every .git directory
    rbh-find rbh:mongo:test -type d -name '.git'

    # find files modified today
    rbh-find rbh:mongo:test -mtime 0

    # find setuid bit
    rbh-find rbh:mongo:test -perm /u+s

.. [#] to set up a backend, have a look at rbh-sync_'s documentation
.. _rbh-sync: https://github.com/cea-hpc/rbh-sync

The following examples all showcase the use of the ``-size`` predicate.

.. code:: bash

    # list all entries with a size of exactly 2 Gigabytes
    rbh-find rbh:mongo:test -size 2G
    /testA

    # list all entries with a size greater than 1 Megabytes
    rbh-find rbh:mongo:test -size +1M
    /
    /testA
    /testB
    /testC

    # list all entries with a size greater than 1 Megabytes and smaller
    # than 1 Gigabytes
    rbh-find rbh:mongo:test -size +1M -size -1G
    /testB

Paths vs. URIs
--------------

The most obvious difference between find and rbh-find is the use of URIs instead
of paths:

.. code:: bash

    find /scratch -name '*.txt'
    rbh-find rbh:mongo:scratch -name '*.txt'

rbh-find queries `RobinHood backends`_ rather than locally mounted filesystems.
The canonical way to refer to backends and the entries they manage are URIs.
Hence rbh-find uses URIs rather than paths.

For more information, please refer to the RobinHood library's `documentation on
URIs`__.


.. _RobinHood backends: https://github.com/cea-hpc/librobinhood/blob/main/doc/internals.rst#backend
.. __: https://github.com/cea-hpc/librobinhood/blob/main/doc/internals.rst#uri

FS traversal vs. Backend filtering
----------------------------------

gnu-find can be compared to a configurable sorting machine.

For example, when running the following command:

.. code:: bash

    find -type f -name '*.txt' -print

The first thing find does is build a tree -- or rather, a pipeline -- of its
command line's predicates (``-type f``, ``-name '*.txt'``) and actions
(``-print``)::

                                           true  --------- (always) true  -----
                                              -->| print |--------------->| ø |
                   true  -------------------  |  ---------                -----
                      -->| name =~ ".txt$" |--|
    ----------------  |  -------------------  |  -----
    | type == FILE |--|                       -->| ø |
    ----------------  |  -----            false  -----
                      -->| ø |
                  false  -----

Then it traverses the current directory (because "." is implied), and its
subdirectories, and their subdirectories, ... And each filesystem entry it
encounters goes through the pipeline. Once.

Now, find allows you to place multiple actions on the command line:

.. code:: bash

    find -print -print

This is also converted into a single tree::

    --------- (always) true  --------- (always) true  -----
    | print |--------------->| print |--------------->| ø |
    ---------                ---------                -----

And each entry is still only processed once (it is printed twice, but iterated
on once).

rbh-find works a little differently. Since it uses RobinHood backends, it can
query all the entries that match a set of predicates at once, rather than
traverse a tree of directories looking for them. But it cannot ask the backend
to run actions on those entries: it has to perform them itself.

The execution flow looks like this::

    ---------   ----------
    | query |-->| action |
    ---------   ----------

And when there are multiple actions::

    -----------   ------------   -----------   ------------
    | query-0 |-->| action-0 |-->| query-1 |-->| action-1 |
    -----------   ------------   -----------   ------------

Where ``query-1`` is a combination of ``query-0`` and whatever predicates appear
between ``action-0`` and ``action-1``.

Another approach would be to fall back to a regular find pipeline after
``action-0``. But this would require reimplementing all the filtering logic of
find, and there is no garantee that it would be faster than issuing a new query.
So rbh-find does not do it that way.

But what are the consequences of such a choice?

There are three:

#. for every action, rbh-find sends one query per URI on the command line;
#. rbh-find's output is not ordered the same way find's is;
#. rbh-find's actions do not filter out any entries.

An example of the difference in the output ordering:

.. code:: bash

    find -print -print
    ./a
    ./a
    ./a/b
    ./a/b
    ./a/b/c
    ./a/b/c

    rbh-find rbh:mongo:test -print -print
    ./a
    ./a/b
    ./a/b/c
    ./a
    ./a/b
    ./a/b/c


The third difference is probably the most problematic. In all the previous
examples, we used the action ``-print`` which always evaluates to ``true`` and
so does not filter out any entries. But there are other actions that do exactly
that:

.. code:: bash

    # find every file that contains 'string'
    find -type f -exec grep -q 'string' {} \; -print

The same query, ran with rbh-find would simply print each file and directory
under the current directory. Implementing the same behaviour as find is not
impossible: it would simply require keeping track of entries that "failed"
actions and exclude them from the next queries. But remembering those entries
could prove prohibitively expensive in terms of memory consumption. Moreover the
time to build the queries would increase as we exclude more and more entries.

-amin, -cmin, and -mmin
-----------------------

find's ``-[acm]min`` predicates do not work quite like ``-[acm]time`` in terms
of how the time boundaries are computed. There is no apparent reason for this.

rbh-find uses the same method for all 6 predicates which it borrows from find's
``-[acm]time``.

-size
-----------------------

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

Extra features
==============

-count
------

rbh-find defines a ``-count`` action that pretty much does what you would
expect: count the matching entries.

.. code:: bash

    # count the file with a '.c' or '.h' extension
    rbh-find rbh:mongo:test -type f -name '*.c' -o -name '*.h' -count
    71 matching entries

**The message format is not yet stable. Please do not rely on it.**
