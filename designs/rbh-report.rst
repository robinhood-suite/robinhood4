.. This file is part of the RobinHood Library
   Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

##########
rbh-report
##########

This design document pertains to the rbh-report tool. Its goal is to provide
statistics about the usage of a storage system, whether it's a filesystem,
object store or database.

Functionning
============

rbh-report will be a new tool based on the librobinhood library to gather
statistics about its different backends. It will use the filtering mechanic of
those backends to retrieve the statistics and create custom reports on them.
Therefore, the command will only be usable on backends that can be filtered, for
instance the Mongo backend (i.e. backends that `rbh-find` can be called upon).

For Mongo specifically, the filtering mechanism used by `rbh-find` is already
needed for rbh-report, i.e. the aggregation. It works by being given a filter
to match specific entries, and options to modify the output. Therefore, we only
need to improve this method to be able to give more advanced queries, like
adding values between entries or searching the maximum.

Thus, the tool will be given 4 types of information:

 * A URI_
 * Filters_ to narrow the search
 * Options_ to aggregate information
 * Statistics_ to print

Throughout this document, we will explain what needs to be done for the
rbh-report of RobinHood V4 with respect to RobinHood V3's rbh-report, and thus
vis-à-vis of Lustre, but similar mechanisms should be applicable to any of the
backends.

URI
---

The URI will work in the exact same manner as for every other tool of the
RobinHood v4 suite (and as described in the internals document__ of
librobinhood).

__ https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#uri

Filter
------

The filters will restrict the entries on which the statistics are computed.
They will be the exact same as the ones provided by rbh-find__, and will handled
and computed in the same manner (i.e. `-size +3G` will have the same meaning
in rbh-find__ and rbh-report).

__ https://github.com/robinhood-suite/robinhood4/blob/main/rbh-find/README.rst

To provide these filters, we will externalize the management of filters from
rbh-find to librobinhood, allowing both rbh-find and rbh-report to use the same
filters.

Options
-------

The options will allow the user to specify how information should be retrieved
and aggregated. These mainly correspond to:
 * grouping information, for instance for a filesystem, by user or by group.
 * sorting information, for instance by the number of entries, by the average
   size, by size ratio, ...

Statistics
----------

The statistics returned by the tool should be at least on par with what
RobinHood V3 provides. It should thus be about the number of inodes, sizes, time
statistics (how many files older than X or younger than Y for instance), etc.

Moreover, the information returned should be displayable in two formats: one
human-readable and one easily parsable.

Also, rbh-report should provide flags to retrieve general statistics about the
backend's usage, for instance to know the largest directories, largest files,
and so on.

Examples of such command options include:

.. code:: bash

    rbh-report rbh:mongo:blob -output stripe-count,stripe-size,sum(size)
    rbh-report rbh:mongo:blob -output hsm-state,count()
    rbh-report rbh:mongo:blob -output user,min(size),max(size),avg(size)

Genericity
----------

Since rbh-report will use librobinhood to interrogate the backends, the tool
and syntax should be generic enough to handle all readable backends. However,
since the filters themselves are not generic, and thus the data outputted isn't
either, a similar code structure to rbh-find and its overloads will be
necessary.

Future proofing
---------------

As different fields will be added during the lifespan of the tool, they should
be easily usable by rbh-report.

The parsing of command line arguments for filters will be common to both
rbh-find and rbh-report. This avoids the risk of having differences between
both tools.

For the statistics printing, this can be done by abstracting the fields used to
retrieve/use. For instance, if we want to get all the entries that have a stripe
count between 1 and 2, 3 and 4, and 5+, the field and intervals can be
abstracted and given by the user in the following manner:

.. code:: bash

    rbh-report rbh:mongo:blob -user test -group-by stripe-count -ratio "[1-2, 3-4, 5+]"

Response time
-------------

With regard to RobinHood V3, the response time of the requests is extremely
important. Many requests of the V3 can be answered instantly, so the rbh-report
of RobinHood V4 should be have similar response time.

To do this, RobinHood V3 uses multiple tables in its database that aggregates
the results which will be requested later by rbh-report. Those tables are
updated with each full scan and update from rbh-fsevents.

Since RobinHood V4 can handle multiple backends for recording information, each
will have to implement its own mechanism, but we will detail here how it can be
done for databases.

For SQL databases, the same behaviour as RobinHood V3 can be used, with specific
tables to hold aggregated information.

For NoSQL databases, we can use the collections mechanism. For instance, in the
Mongo backend, we allow the usage of different databases which can be targeted
by the URI (i.e. `rbh:mongo:test_db`). Inside of those databases, we target
one specific collection (currently, `entries`) and write information to it.

Therefore, to report information faster, we can use another collection in it,
for instance the `stats` collection. This way, we can aggregate different
information in different collections, and requests them instantly, as only
those information will be in that collection, making it a few document at most.

Ease of use
-----------

As the commands used for invoking rbh-report (and other tools) can get quite
lengthy, we propose adding `profiles` to the configuration of rbh-report. They
will work like aliases for commands, and will be specified as follow in the
configuration file:

.. code:: YAML

    ---
    profiles:
        large_files: -size +1T -group-by user,size -output count()
    ---

On the other side, when invoking the command, this alias will be used as
follows:

.. code:: bash

    rbh-report rbh:mongo:blob --profile large_files
