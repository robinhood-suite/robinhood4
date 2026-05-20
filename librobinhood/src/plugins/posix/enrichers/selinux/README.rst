.. This file is part of RobinHood
   Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

#################
SELinux extension
#################

The SELinux extension is an enricher for the POSIX plugin.
It is comprised of two parts: the information enrichment and the information
filtering and printing.

The first part is mainly used by rbh-sync_ and rbh-fsevents_.
The second part is used by rbh-find_ and rbh-report_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

SELinux overview
================

SELinux is an implementation of mandatory access control (MAC) for Linux. It
adds an additional access control layer on top of traditional Unix permissions:
even when standard permissions allow an operation, SELinux policy rules may
still deny it. SELinux labels processes and filesystem objects with a SELinux
context, which usually follows this form::

    user:role:type:range

The `user` field is the SELinux user, which is distinct from the regular Linux
user account. The `role` field is used by Role-Based Access Control (RBAC).
The `type` field is used by Type Enforcement (TE). For example, a process
running in one domain may only access object types explicitly allowed by the
policy.

The last field, the `range` is used by MLS/MCS mechanisms.
In common `targeted` policies, this field is often a simple
level such as `s0`. Multi-Category Security (MCS) can add categories on this
level, for example `s0:c123,c456`. Multi-Level Security (MLS) can express
low-high ranges with sensitivity levels and categories, for example
`s0:c1-s4:c1,c4,c7.c9`.

For MLS/MCS comparisons, a level is said to dominate another one when its
sensitivity is greater than or equal to the other sensitivity and its category
set includes all categories of the other level. For example, ``s4:c1,c4,c7.c9``
dominates ``s2:c8`` because ``s4`` is higher than ``s2`` and ``c8`` is included
in ``c7.c9``.

Enrichment
==========

Here are the additional information enrichment for the POSIX plugin:
 * For all entries with a ``security.selinux`` extended attribute:
   * Complete SELinux context
   * SELinux user
   * SELinux role
   * SELinux type
   * SELinux range

The SELinux context is read from the ``security.selinux`` extended attribute.
It is expected to follow this format::

        user:role:type:range

For instance::

        system_u:object_r:container_file_t:s0:c0,c2-s3:c0.c2,c7

Is stored as::

        selinux.context =
                  system_u:object_r:container_file_t:s0:c0,c2-s3:c0.c2,c7
        selinux.user = system_u
        selinux.role = object_r
        selinux.type = container_file_t
        selinux.range = s0:c0,c2-s3:c0.c2,c7

The range field keeps everything after the third ':' so MLS/MCS contexts
remain properly stored and reconstructible.


Filtering
=========

The SELinux extension defines predicates and directives that will be used with
rbh-find_ and rbh-report_. The predicates define filters that will be usable by
users to filter data retrieved from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

Below are listed the predicates defined by the SELinux extension.

-selinux-context
----------------

A predicate that filters entries which have the given complete SELinux context.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-context \
        system_u:object_r:httpd_sys_content_t:s0
    ./dir/httpd-file

-selinux-user
-------------

A predicate that filters entries which have the given SELinux user.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-user system_u
    ./dir/system-file

-selinux-role
-------------

A predicate that filters entries which have the given SELinux role.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-role object_r
    ./dir/httpd-file

-selinux-type
-------------

A predicate that filters entries which have the given SELinux type.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-type httpd_sys_content_t
    ./dir/httpd-file

-selinux-range
--------------

A predicate that filters entries which have the given SELinux range.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-range s0:c123,c456
    ./dir/container-file

-selinux-range-dominates
------------------------

A predicate that filters entries that dominate a given SELINUX range.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-range-dominates s0:c123,c456
    ./dir/container-file

-selinux-range-dominated-by
---------------------------

A predicate that filters entries that are dominated by a given SELINUX range.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-range-dominated-by s0:c123,c456
    ./dir/container-file


Printing
========

The SELinux extension defines directives that will be used with rbh-find_.
They correspond to which data is printed when using the `-printf` and
`-fprintf` actions of rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Below are defined the directives allowed by the SELinux extension.

+--------------+-------------------------------------------+
|  Directive   |   Information printed                     |
+==============+===========================================+
|    '%RZc'    |   The complete SELinux context            |
+--------------+-------------------------------------------+
|    '%RZu'    |   The SELinux user                        |
+--------------+-------------------------------------------+
|    '%RZr'    |   The SELinux role                        |
+--------------+-------------------------------------------+
|    '%RZt'    |   The SELinux type                        |
+--------------+-------------------------------------------+
|     %RZR'    |   The SELinux range                       |
+--------------+-------------------------------------------+
