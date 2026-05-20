.. This file is part of RobinHood
   Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

################
SELinux extension
################

The SELinux extension is an enricher for the POSIX plugin.
It is comprised of two parts: the information enrichment and the information
filtering and printing.

The first part is mainly used by rbh-sync_ and rbh-fsevents_.
The second part is used by rbh-find_ and rbh-report_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

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
        
        system_u:object_r:container_file_t:s0:c123,c456

is stored as::

        selinux.context = system_u:object_r:container_file_t:s0:c123,c456
        selinux.user = system_u
        selinux.role = object_r
        selinux.type = container_file_t
        selinux.range = s0:c123,c456

The range field keeps everything after the third ':' so MLS/MCS contexts
remain properly stored and reconstructible.


Filtering
=========

The SELinux extension defines predicates and directives that will be used with
rbh-find_ and rbh-report_. The predicates define filters that will be usable by
users to filter data retrieved from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

Below are defined the predicates defined by the SELinux extension.

-selinux-context
-------

A predicate that filters entries which have the given complete SELinux context.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-context \
        system_u:object_r:httpd_sys_content_t:s0
    ./dir/httpd-file

-selinux-user
-------

A predicate that filters entries which have the given SELinux user.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-user system_u
    ./dir/system-file

-selinux-role
-------

A predicate that filters entries which have the given SELinux role.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-role object_r
    ./dir/httpd-file

-selinux-type
-------

A predicate that filters entries which have the given SELinux type.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-type httpd_sys_content_t
    ./dir/httpd-file

-selinux-range
-------

A predicate that filters entries which have the given SELinux range.

.. code:: bash

    rbh-find rbh:mongo:test -selinux-range s0:c123,c456
    ./dir/container-file


Printing
========

The SELinux extension defines directives that will be used with rbh-find_.
They correspond to which data is printed when using the `-printf` and
`-fprintf` actions of rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Below are defined the directives allowed by the SELinux extension.

+-------------+--------------------------------------------+
|  Directive   |   Information printed                     |
+=============+============================================+
|     '%Z'     |   The complete SELinux context            |
+-------------+--------------------------------------------+
|     '%Zu'    |   The SELinux user                        |
+-------------+--------------------------------------------+
|     '%Zr'    |   The SELinux role                        |
+-------------+--------------------------------------------+
|     '%Zt'    |   The SELinux type                        |
+-------------+--------------------------------------------+
|     '%ZR'    |   The SELinux range                       |
+-------------+--------------------------------------------+
