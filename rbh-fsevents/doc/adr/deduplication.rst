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

.. TODO
