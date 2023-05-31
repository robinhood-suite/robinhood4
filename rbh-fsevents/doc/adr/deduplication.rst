.. This file is part of rbh-fsevents
   Copyright (C) 2020 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#######################
Deduplication algorithm
#######################

Context
=======

Filesystems under heavy use can absorb several billion metadata updates in a
single day, sometimes millions in as little as a minute.

Fortunately, most of these updates can be coalesced into a lot less fsevents_.

For example, consider the following program:

.. code:: python

    from os import fdatasync

    def update(path, value):
        with open(path, 'a') as stream:
            stream.write(str(value))
            stream.flush()
            fdatasync(stream.fileno())

    def my_complex_algorithm(step, *args, **kwargs):
        # Some complex algorithm which computes a very important
        # metric you want to commit on disk as soon as possible
        return 0 # TODO

    if __name__ == '__main__':
        for step in range(1 << 30):
            update('my_important_metric', my_complex_algorithm(step))

If run, this program will issue a little over a billion ``open()`` and
``close()`` system calls, which, on a filesystem such as Lustre, will generate
just as many metadata updates.

Of course, the program would much better be written:

.. code:: python

    from os import fdatasync

    def my_complex_algorithm(step, *args, **kwargs):
        # Still some complex algorithm which computes a very important
        # metric you want to commit on disk as soon as possible
        return 0 # TODO

    if __name__ == '__main__':
        with open('my_important_metric', 'w') as stream:
            for step in range(1 << 30):
                stream.write(str(my_complex_algorithm(step)))
                stream.flush()
                fdatasync(stream.fileno())

But sometimes, it is not, and we have to deal with it.

Now, we have a billion filesystem events on our hands, but do we care equally as
much about each of them? Not really. For a system such as RobinHood, seeing all
those events, or just the last one, is just as informative.

Therefore, ``rbh-fsevents`` should be able to coalesce and deduplicate fsevents
on the fly as much as possible.

.. _fsevents: https://github.com/cea-hpc/librobinhood/blob/main/doc/internals.rst#fsevent

Requirements
============

Controlled divergence
---------------------

To be useful, a RobinHood backend should be as up-to-date as possible.

There should be a configurable upper limit to how much ``rbh-fsevents`` can
delay the processing of filesystem events for the sake of deduplicating them.

Catching up
-----------

In some situations, the previously described maximum delay could lead RobinHood
to never catching up with a large backlog of fsevents.

For example, if ``rbh-fsevents`` is not run for a minute, and the configured
maximum delay is set to a few seconds, it will lead to ``rbh-fsevents``
processing fsevents one at a time. This would make it unlikely that
``rbh-fsevents`` ever catches up with the event source.

There should be a configurable lower limit to how many fsevents should at least
be processed in one pass. Of course, if less events are available than this
limit, ``rbh-fsevents`` should go ahead and process the events immediately.

Memory constraints
------------------

There should be a configurable upper limit to how much memory ``rbh-fsevents``
is allowed to use.

Efficiency
----------

As long as every other constraint is met, there should be no limit to how many
fsevents can be deduplicated at once. This allows ``rbh-fsevents`` to be as
efficient as possible.

Acknowledgment
--------------

To ensure no event is lost, most, if not all event sources supported by
``rbh-fsevents`` will provide a transactional mean to acknowledge the processing
of a batch of fsevents.

The deduplication algorithm must allow determining which fsevents have been
processed durably and which fsevents have not.

Solution
========

In broad terms, this solution relies on a map of entries that will be updated
and deduplicated with each new event created by the source. When that map hits a
certain threshold, part of its entries will be given to the enricher for
processing and sink for syncing.

Map of inodes
-------------

When an event is managed by the source, it will output one or severals fsevents.
These fsevents are kept for progressive deduplication. To keep them, we will use
a hashmap with keys corresponding to the fsentries's ``rbh_id`` targeted by the
fsevents.

When multiple fsevents target the same fsentry, they will be deduplicated.
However, since the fsevents may be of different nature, they cannot just be
assembled into one final event. Instead, we will use a list of fsevents. Some
general rules about the deduplication of that list will be explained later.

This list of fsevents will be per ``rbh_id``, and will thus constitute the
values associated with the keys in the hashmap. It may also be interesting to
instead keep a deduplication specific structure to encapsulate this list of
fsevents, which will be usefull for later improvements.

Flushing the map
----------------

In order to decide when the map should be flushed, the caller of the
``rbh-fsevents`` command will have to provide a size for this map, corresponding
to the maximum number of fsentries that can be contained. When that number
reaches the maximum, part of the map will be flushed.

At that point, a full array of fsevents will be given back to the enricher.
Currently, that array can only be of size 1, meaning we treat fsevents one by
one, but we can obtain better performances by instead giving the full list of
fsevents that will be flushed, thus flushing for instance 300 fsevents at once.

Flushing only a part of the map means we can keep the other parts of the map
ready for deduplication, and keep deduplicating events perpetually.

Least Recently Used flushing
-----------------------------

To decide which fsentries should be flushed, we can in a first version simply
take N entries at random, another more performant idea is to use a LRU-type
flush. To do so, we need to keep a record of when each fsentry was added to
the hashmap.

This can be done by using a doubly-linked list. When an fsentry is added to the
map, its corresponding ``rbh_id`` is also added as a node to the front of the
list. When it is updated, the node is moved to the front.

With this list, when we want to flush a certain number of fsentries, we will
only have to start from the back of the list and take that number of events in
it. That means we will flush the least recently used entries, while keeping the
most recently updated ones around for deduplication in the following batches of
source events.

For this improvement, it is necessary that the main deduplication structure
(which contains the hashmap) contains the reference to the front and back nodes
of the list, but it is also necessary that each value associated to a fsentry
(key) in the hashmap keeps the reference to its own node in that list, so that
it can be updated easily when a new fsevent concerns that fsentry.

Deduplication rules
-------------------

We will not go here into details about the deduplication rules, but general
rulings should go as such:
* if two events of the type ``RBH_FET_UPSERT`` or ``RBH_FET_XATTR`` are found,
they should be deduplicated into one.
* if a ``RBH_FET_LINK`` event is found and the corresponding ``rbh_id`` does not
exist in the map, the key/value should be created, but if it already exists, the
event should simply be added to the list, as it also corresponds to hardlinks.
This means we may have multiple link fsevents per ``rbh_id``.
* conversely, if a ``RBH_FET_UNLINK`` is found, it should only remove the
corresponding ``RBH_FET_LINK`` event if possible, otherwise simply add the
fsevent to the targeted fsentry list.

Parameters
----------

3 parameters will be available for this deduplication process:

* a number of source events to manage: currently, the ``rbh-fsevents`` command
runs indefinitely, as long as there are events in the source to manage. Once the
source runs out of events, the command stops. This parameter will provide an
upper limit on the number of source events a single call will manage. Still, if
the source has no more event to manage, the command will stop. For instance, the
command will manage 500k events, or, if given "unlimited" or not mentionned,
will read as many events as it can.

* the map length: the number of fsentries kept in the map. Once the map attains
that size, a certain number of entries will be flushed to the enricher. For
instance, the map will contain 50k entries.

* the pourcentage to flush: the pourcentage of entries of the map to flush when
it attains full capacity. For instance, when the map is full, flush 50% of its
entries.
