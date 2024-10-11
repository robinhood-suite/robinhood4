.. This file is part of the RobinHood Library
   Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

##########
rbh-report
##########

This design document pertains to the rbh-report tool. It's goal is to provide
statistics about the usage of a storage system, whether it's a filesystem,
object store or database.

Functionning
============

rbh-report will be a new tool based on the librobinhood library to gather
statistics about its different backends. It will use the filtering mechanic of
those backend to retrieve the statistics and create custom reports on them.

Thus, the tool will be given 4 types of information:

 * An URI_
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

The filters will allow the user to select what the statistics outputted should
target. They will be specified just like rbh-find__ filters, and may rely on the
same mechanics as that tool. Moreover, the allowed filters will be the exact
same as the ones provided by rbh-find__.

__ https://github.com/robinhood-suite/robinhood4/blob/main/rbh-find/README.rst

To provide these filters, we will externalize the management of filters from
rbh-find to librobinhood, allowing both rbh-find and rbh-report to use the same
filters.

Options
-------

The options will allows the user to precise how information should be retrieved
and aggregated. These mainly correspond to:
 * grouping information, for instance for the filesystem, by user or by group.
 * sorting information, for instance by the number of entries, by the average
   size, by size ratio, ...

Statistics
----------

The statistics returned by the tool should be at least on par with what
RobinHood V3 provides. It should thus be about the number of inodes, sizes, time
statistics (how many files older than X or younger than Y, ...), ...

Moreover, the information returned should be displayable in two formats: one
human-readable and one easily parsable.

Also, rbh-report should provide flags to retrieve general statistics about the
backend's usage, for instance to know the largest directories, largest files,
and so on.

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

Regarding the filtering for those fields, it will be done thanks to the
gathering of the filters in one place, so that both rbh-report and rbh-find can
use them, thus needing no additional development.

For the statistics printing, this can be done by abstracting the fields used to
retrieve/use. For instance, if we want to get all the entries that have a stripe
count between 1 and 2, 3 and 4, and 5+, the field and intervals can be
abstracted and given by the user in the following manner:

.. code:: bash

    rbh-report rbh:mongo:blob -user test -group-by stripe-count -by-ratio [1-2, 3-4, 5+]

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
by the URI (i.e.  `rbh:mongo:test_db`). Inside of those databases, we target
one specific collection (currently, `entries`) and write information to it.
Therefore, to report information faster, we can use another collection in it.
This way, we can aggregate different information in different collections, and
requests them instantly, as only those information will be in that collection,
making it a few document at most.
