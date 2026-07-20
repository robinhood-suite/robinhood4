.. This file is part of RobinHood
   Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

#############
ACL extension
#############

The ACL extension is an enricher for the POSIX plugin. It is comprised of two
parts: the information enrichment and the information filtering and printing.

The first part is mainly used by rbh-sync_ and rbh-fsevents_.
The second part is used by rbh-find_ and rbh-report_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report


Enrichment
==========

Here are the additional information enrichment for the POSIX plugin:

 * For POSIX access ACLs:

   * owning group ACL permissions;
   * named user ACL entries;
   * named group ACL entries.

 * For POSIX default ACLs:

   * default owner permissions;
   * default group permissions;
   * default mask permissions;
   * default other permissions;
   * default named user ACL entries;
   * default named group ACL entries.

POSIX ACLs are read from the following extended attributes::
    system.posix_acl_access
    system.posix_acl_default

The parsed metadata is stored under::

    acl

Access ACLs avoid duplicating information already represented by
`statx.mode`. The owner permissions, the ACL mask and the other
permissions are already represented by the mode bits. The extension only
stores the owning group ACL entry and named user or group entries.

For instance, this ACL entry::

    user:424242:rw-

is stored under `acl.access.users` as::

    acl.access.users: [
        { id: 424242, p: 6 }
    ]

Named group entries are stored the same way under `acl.access.groups`.

In both cases, `id` is respectively the UID or GID and `p` is the
POSIX ACL permission bitmask.

Default ACLs are not represented by `statx.mode` and are therefore
stored explicitly.


Filtering
=========

The ACL extension defines predicates and directives that will be used with
rbh-find_ and rbh-report_. The predicates define filters that will be usable by
users to filter data retrieved from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

Below are listed the predicates defined by the ACL extension.

-acl-user
---------

A predicate that filters entries which have an access ACL entry for a
given named user.

.. code:: bash

    rbh-find rbh:mongo:test -acl-user 424242
    ./dir/user-acl-file

-acl-group
----------

A predicate that filters entries which have an access ACL entry for a
given named group.

.. code:: bash

    rbh-find rbh:mongo:test -acl-group 434343
    ./dir/group-acl-file

-acl-defaul-user
----------------

A predicate that filters directories which have a defaul ACL entry for a
given named user.

.. code:: bash

    rbh-find rbh:mongo:test -acl-default-user 424242
    ./dir-acl-user

-acl-defaul-group
----------------

A predicate that filters directories which have a defaul ACL entry for a
given named group.

.. code:: bash

    rbh-find rbh:mongo:test -acl-default-user 434343
    ./dir-acl-group

-acl-[readable|writable|executable]
-----------------------------------

A predicate that filters entries which are [readable|writable|executable] by
a given subject according to UNIX mode bits and POSIX access ACLs.

The subject is written with a UID and a list of GIDs::
    UID:GID[,GID...]
This form allows the check of the full ACL access algorithm.

.. code:: bash

    rbh-find rbh:mongo:test -acl-readable 424242:23456,38363
    ./dir/readable-file
