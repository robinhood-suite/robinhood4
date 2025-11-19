.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

################
Lustre extension
################

The Lustre extension is an enricher for the POSIX plugin.
It mainly retrieves Lustre specific information about an entry, and enrich the
corresponding FSEntry.

The enricher is mainly used by rbh-sync_ and rbh-fsevents_.

.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync

Enrichment
==========

Here are the additional information enrichment for the POSIX plugin:
 * For regular files:
   * HSM state if they have a HSM state
   * HSM archive ID if they have a HSM state
   * magic number
   * layout generation
 * For directories and regular files:
   * MDT index
 * For directories:
   * MDT count
   * MDT index
   * MDT hash type
   * MDT hash flags
 * For each entry that isn't a symbolic link:
   * FID
   * layout flags
   * mirror count for composite entries
   * project ID
   * per component (composite entries have multiple components, non-composite
     have a single one):
     * stripe_count
     * stripe_size
     * pattern
     * component flags
     * pool
     * ost
     * if the file is composite, 3 more attributes:
       * mirror_id
       * begin
       * end

Filtering
=========

The Lustre extension defines predicates that will be used with rbh-find_. The
predicates define filters that will be usable by users to filter data retrieved
from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Below are defined the predicates defined by the Lustre extension.

-comp-end
---------

A predicate that filters entries based on the component end of any of their
components. It follows the logic of a size filter, and thus will filters entries
following this logic:

.. code:: bash

    rbh-find rbh:mongo:test -comp-end 5M # -> filter entries that have a
    # component end within the interval ]4M ; 5M]

    rbh-find rbh:mongo:test -comp-end +10M # -> filter entries that have a
    # component end within the interval ]10M ; +oo[

    rbh-find rbh:mongo:test -comp-end -10M # -> filter entries that have a
    # component end within the interval [0 ; 9M]

Be wary that filtering entries using this predicate twice will NOT filter
entries that a component end that matches the first predicate AND the second
predicate at the same time. Therefore, ``-comp-end +10M -comp-end -20M``
will not give entries that a component end both superior to 10M and inferior
to 20M. Rather, it will output entries with that have a component end
superior to 10M and a component end inferior to 20M, whether they are the
same component start or not.

To allow the above expected behaviour, we allow the input field of the
``comp-end`` predicate to be two values separated by a comma. In that case
the first half is expected to be the lower bound of the interval while the
second is the high bound (following the exact filtering behaviour).

Thus, ``comp-end 10M,20M`` will filter entries that have a component end
superior to 9M and inferior or equal to 20M.

To showcase the difference:

.. code:: bash

    lfs setstripe -E 1G -S 256k -E -1 -S 512k "comp_end_at_1G_and_eof"

    rbh-find rbh:mongo:test -comp-end 1G -comp-end +2G
    ./comp_start_at_1G_and_eof # -> this is because the component end at 1G
    # satisfies the first predicate and the component end eof satisfies the
    # second

    rbh-find rbh:mongo:test -comp-end 1M,2M
    # nothing is outputted because `comp_start_at_1G_and_eof` doesn't have a
    # component end in the interval ]0 ; 2M]


-comp-start
-----------

A predicate that filters entries based on the component start or (`begin`)  of
any of their components. It behaves exactly like the ``-comp-end`` predicate
otherwise.

-fid
----

A predicate that filters entries based on a fid.

.. code:: bash

    rbh-find rbh:mongo:test -fid [0xa:0xb:0xc]
    /dir/file-1

-hsm-state
----------

A predicate that filters entries based on their HSM status. These include:
archived, dirty, exists, lost, noarchive, norelease, none and released.

.. code:: bash

    rbh-find rbh:mongo:test -hsm-state archived
    /file
    /dir/file-0
    /dir/file-1

    rbh-find rbh:mongo:test -hsm-state archived -hsm-state norelease
    /dir/file-0

-layout-pattern
---------------

A predicate that filters entries based on the pattern of any of their
components. The only accepted values are ``default``, ``raid0``, ``mdt``,
``released`` and ``overstriped``. It behaves exactly like the ``stripe-count``
predicate with regards to the default value otherwise.

-mdt-count
----------

A predicate that filters directories based on how many MDTs they are striped
across. It supports filtering on an MDT count equal to, greater than, or less
than ``n``, by using ``n``, ``+n``, or ``-n``, respectively.

.. code:: bash

    lfs mkdir -c 3 dir-on-3-mdt

    rbh-find rbh:mongo:test -mdt-count 3
    /dir-on-3-mdt

-mdt-index
----------

A predicate that filters entries based on an MDT index.

.. code:: bash

    rbh-find rbh:mongo:test -mdt-index 1
    /dir-on-mdt-1
    /dir-on-mdt-1/file

-ost-index
----------

A predicate that filters entries based on an OST index.

.. code:: bash

    rbh-find rbh:mongo:test -ost 1 -o ost 2
    /dir/file-on-ost-1
    /dir/file-on-ost-2
    /dir/file-on-ost-1-and-2

-[i]pool
--------

A predicate that filters entries based on the pool their components belong to.
It supports regular expressions and is case-sensitive by default. To make it
case-insensitive, use the ``-ipool`` predicate.

.. code:: bash

    lfs setstripe -E 1M -p "pool1" -E -1 -p "pool2" file

    rbh-find rbh:mongo:test -pool pool1
    /file
    rbh-find rbh:mongo:test -pool pool*
    /file

-project-id
-----------

A predicate that filters entries based on a project ID.

.. code:: bash

    rbh-find rbh:mongo:test -project-id 1
    /dir-for-project1
    /dir-for-project1/file-0
    /dir-for-project1/file-1

-stripe-count
-------------

A predicate that filters entries based on the stripe count of any of their
components. It supports filtering on a stripe count equal, larger or lower than
``n``, by respectively using ``n``, ``+n`` or ``-n``.

Moreover, the value given can be ``default``, in which case it will filter all
that do not have a stripe count set. This will only match directories however,
because only directories have a default striping, as it does not make sense for
other types of entries in Lustre.

Finally, if given a numerical value, rbh-find will fetch the default stripe
count of the current mount point (meaning we assume rbh-find's working
directory is under the right filesystem mount point), and compare it to the
value that is to be filtered. If they match (i.e. if the default stripe is
equal, larger or lower then the value given by the user), every directory with
a default stripe count will be printed.

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

A predicate that filters entries based on the stripe size of any of their
components. It behaves exactly like the ``stripe-count`` predicate otherwise.
