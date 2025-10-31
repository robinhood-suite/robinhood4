.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

############
rbh-fsevents
############

rbh-fsevents collects and processes storage system events to keep a RobinHood
backend up to date.

Overview
========

rbh-fsevents captures storage system events and updates a `RobinHood Backend`_
[#]_. It supports multiple event sources and can enrich events with additional
metadata (xattrs, Lustre-specifc information, etc.) before updating the
destination backend.

.. [#] simply put, a storage backend which RobinHood can use to store a
       filesystem's metadata, and later query it.

.. _RobinHood Backend:
       https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#backend

Usage
=====

The basic syntax for rbh-fsevents is:

::

    rbh-fsevents SOURCE DESTINATION

The **SOURCE** can be either a event source or stdin, and the **DESTINATION**
can be either a backend or stdout.

Examples:

.. code:: bash

    rbh-fsevents src:lustre:lustre-MDT0000 rbh:mongo:test
    rbh-fsevents src:yaml:/path/to/events rbh:mongo:test
    rbh-fsevents - < /tmp/fsevents.yaml rbh:mongo:test
    rbh-fsevents src:lustre:lustre-MDT0000 - > /tmp/fsevents.yaml

URIs
====

rbh-fsevents uses RobinHood URIs to identify the backends.

Source URIs
-----------

rbh-fsevents uses source URIs to identify the event sources.

The general syntax of a source URI is:

::

    src:<type>:<identifier>[?options]

Example:

::

    the scheme        the source identifier
           vvv        vvvvvvvvvvvvvv
           src:lustre:lustre-MDT0000?ack-user=cl1
               ^^^^^^                ^^^^^^^^^^^^
            the source type             options

Supported source types:
* ``Lustre``: MDT changelogs
* ``file``:   YAML file

The syntax for the source identifier depends on the source type:

* for ``lustre`` it must be the name of a MDT;
* for ``yaml`` it must be the path to a yaml file.

The options accepted on a source URI depend on the source type (e.g ack-user
for the lustre source)

Enrichment
==========

By default, only the metadata available in source events are stored. You can use
the ``--enrich`` option to fetch all metadata before storing it in the
destination backend. The ``--enrich`` option takes as input a RobinHood URI
representing the backend to use for metadata enrichment. The information
retrieved depends on the backend used for enrichment.

Example:

.. code:: bash

    rbh-fsevents --enrich rbh:lustre:/mnt/lustre src:lustre:lustre-MDT0000 \
        rbh:mongo:test

Deduplication
=============

rbh-fsevents implements an in-memory deduplication layer that merges multiple
events affecting the same entry before they are stored in the destination
backend.

How it works
------------

Events that target the same entry are grouped in a batch and merged together.
When the batch is full, it is flushed and the events are applied to the
destination backend. The size of the batch can be controlled with the
``--batch-size`` option, which specifies how many entries can be retained for
deduplication at once. By default, the deduplication is enabled and the batch
size is set to 100 entries.

To disable the deduplication, you must set the batch size to 0. Without
deduplication, each event is enriched and applied to the destination
backend. This may significantly increase the load and number of operations
performed on both the enrichment and destination backends.

.. code:: bash

    # With deduplication and batch size of 1000
    rbh-fsevents --batch-size 1000 --enrich rbh:lustre:/mnt/lustre \
        src:lustre:lustre-MDT0000 rbh:mongo:test

    # Without deduplication
    rbh-fsevents --batch-size 0 --enrich rbh:lustre:/mnt/lustre \
        src:lustre:lustre-MDT0000 rbh:mongo:test

Lustre source
=============

One important things to know is that one rbh-fsevents process can only handle
the changelog stream of one MDT at a time. If you have multiple MDTs, you
must launch one rbh-fsevents par MDT.

Acknowledgement
---------------

rbh-fsevents supports acknowledging processed Lustre changelogs. The
acknowledgement allow rbh-fsevents to free space on the MDT. Once acknowledged,
processed changelogs can be safely purged by the MDT, preventing changelog files
from growing indefinitely.

By default, the acknowledgement is disabled. To enable this feature, you must
add the ``ack-user`` option to the Lustre source URI.

::

    src:lustre:lustre-MDT0000?ack-user=cl1

When acknowledgement is enabled, rbh-fsevents saves its progress state so it
can resume processing changelogs after a restart. This is particularly useful
when other external readers consume the same MDT changelogs with a different
speed of acknowledgement. rbh-fsevents guarantees that no changelog record is
processed twice.

.. code:: bash

    # With acknowledgement
    rbh-fsevents --enrich rbh:lustre:/mnt/lustre \
        src:lustre:lustre-MDT0000?ack-user=cl1 rbh:mongo:test

Parallelism
===========

rbh-fsevents can enrich events and update the destination backend in
parallel using a pool of worker threads. It follows a producer/consumer pattern
with a single producer reading and deduplicating the events and N workers
enriching and updating the events. The events are distributed among workers on
the hash of their `RobinHood ID`_.

.. _RobinHood ID:
    https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/include/robinhood/id.h

By default, rbh-fsevents runs with a single worker. To increase parallelism, use
the ``--nb-workers`` option:

.. code:: bash

    # With 8 workers
    rbh-fsevents --nb-workers 8 --enrich rbh:lustre:/mnt/lustre \
        src:lustre:lustre-MDT0000 rbh:mongo:test
