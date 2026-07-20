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
