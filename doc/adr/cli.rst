.. This file is part of rbh-fsevent
   Copyright (C) 2020 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

######################
Command line interface
######################

Requirements
============

Multiple input sources/formats
------------------------------

rbh-fsevent must be able to parse events from different event sources: Lustre,
FANOTIFY, RobinHood, BeeGFS, HPSS, ...

Each of these streams uses a different format and collection API.

One way or another, rbh-fsevent must be able to distinguish one type of stream
from another.

Distributed processing
----------------------

The collection and processing of fsevents is a resource intensive job. It is
therefore important that this task can be split and distributed to several
workers.

Here are the different steps in the collection and processing of fsevents:

.. Explaining what "sparse" means here is out of scope for this document.
   TODO write another document and explain it there, then reference it here.

#. collection of raw external events;
#. conversion to (sparse) fsevents;
#. enrichment of fsevents;
#. applying fsevents on a robinhood backend.

A simple approach to the problem at hand is to distribute each and every step to
a different worker pool. Therefore rbh-fsevent's cli should make it possible to
run each step individually.

In practice, each step can be divided into 3 phases::

    collect/deserialize input --> do something --> serialize/emit output

From this, it becomes clear that step 1 is included in step 2 and should not be
a step of its own.

Multiple output sinks
---------------------

From the previous point, it is obvious that rbh-fsevent must be able to emit
data to multiple output sinks.

We want to support the following:

- a RobinHood backend;
- a message queue;
- pipes / files / stdout.

Solution
========

Here is what the CLI of rbh-fsevent will look like.

.. code:: console

    $ ./rbh-fsevent --help
    usage: rbh-fsevent [-h] [--raw] [--enrich MOUNTPOINT] SOURCE DESTINATION

    Collect fsevents from SOURCE, optionally enrich them with data collected from
    MOUNTPOINT, and send them to DESTINATION.

    Positional arguments:
        SOURCE       can be one of:
                         - a path to a yaml file, or '-' for stdin;
                         - an MDT name (eg. lustre-MDT0000).
        DESTINATION  can be one of:
                         - '-' for stdout;
                         - a RobinHood URI (eg. rbh:mongo:test).

    Optional arguments:
        --enrich MOUNTPOINT  enrich fsevents by querying MOUNTPOINT as needed
        -h, --help           print this message and exit
        --raw                do not enrich fsevents (default)

    Note that uploading raw records to Robinhood backend will fail, they have to
    be enriched first.

Multiple input sources
----------------------

The solution above only mentions ``stdin`` and Lustre as input sources, which
makes it quite easy to distinguish one type of input source from the other (ie.
either the argument is ``-`` and it must be ``stdin``, or it is not, and it
must be the name of a Lustre MDT).

But we are also confident that we can support at least two other input sources:

- FANOTIFY: ``SOURCE`` is a path to a file/directory
- RobinHood: ``SOURCE`` is a robinhood URI (eg. ``rbh:...``)

We leave it as an exercise to others to find something that works for their own
event streams.

Running each processing step individually
-----------------------------------------

As described in the requirements_ the CLI must be able to run each processing
step individually, so that work may be distributed across different worker
pools.

.. code:: console

   $ # Collecting and converting raw external events into (sparse) fsevents
   $ rbh-fsevent lustre-MDT0000 - > /tmp/sparse-fsevents.yaml

   $ # Enriching fsevents
   $ rbh-fsevent --enrich /mnt/scratch - < /tmp/sparse-fsevents.yaml \
       - > /tmp/fsevents.yaml

   $ # Applying fsevents on a RobinHood backend
   $ rbh-fsevent - < /tmp/fsevents.yaml rbh:mongo:scratch

The same result can be achieved without intermediary files using pipes:

.. code:: console

   $ rbh-fsevent lustre-MDT0000 - |
       rbh-fsevent --enrich /mnt/scratch - - |
       rbh-fsevent - rbh:mongo:scratch

Or in a single command:

.. code:: console

   $ rbh-fsevent --enrich /mnt/scratch lustre-MDT0000 rbh:mongo:scratch

What about message queues?
--------------------------

They can easily be supported by adding one more type of ``SOURCE`` and
``DESTINATION``, but I am not sure exactly how that will look like in practice.
I would like to use a URI, but our target message queue -- Kafka -- does not
seem to have its own scheme.

This remains on the TODOLIST for now.
