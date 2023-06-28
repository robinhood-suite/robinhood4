.. SPDX-License-Identifer: LGPL-3.0-or-later

################
Partial fsevents
################

Some filesystems support emitting metadata events which robinhood can use to
update its view of the filesystem. In general these event systems only publish
partial information, which robinhood must then enrich by querying the
filesystem.

Requirements
============

Minimize the load on filesystems
--------------------------------

The enrichment of fsevents is a costly operation for robinhood which generates
additional metadata load on filesystems. It is therefore important that
robinhood minimizes its metadata accesses.

Decouple fsevent collection and enrichment
------------------------------------------

To our knowledge, available fsevent collection interfaces (fanotify, Lustre
changelog records, BeeGFS filesystem modification events, ...) are easiest to
use in a single-threaded context, but fsevent enrichment benefits from
multiprocessing (and workload distribution in the case of distributed
filesystems).

It is therefore important that the "unknown" fields still are serializable and
deserializable, so they can be sent from a single-threaded collector to a pool
of workers which can enrich fsevents in parallel.

Solution
========

One way to achieve this is to allow fsevents to hold partial data until they are
deduplicated and only then to enrich them.

To this end, we suggest using extended attributes.


``rbh-fsevents`` xattr
----------------------

Any xattr stored in a map under the root key ``rbh-fsevents`` shall have meaning
only to rbh-fsevents and never be passed on to robinhood backends.

The map shall contain any of the following keys:

- ``statx``;
- ``xattrs``;
- ``symlink``.


``statx``
~~~~~~~~~

This key may only be present for ``RBH_FET_UPSERT`` fsevents.

The value associated to this key must be a sequence of zero or more statx
fields which need enriching.

Example:

.. code:: YAML

    --- !upsert
    id: !!binary aS1hbS1hLWZpbGUtaGFuZGxlCg==
    xattrs:
        rbh-fsevents:
            statx:
              - type
              - mode
              - nlink
              - uid
              - gid
              - atime:
                - sec
                - nsec
              - mtime:
                - sec
                - nsec
              - ctime:
                - sec
                - nsec
              - ino
              - size
              - blocks
              - btime:
                - sec
                - nsec
              - blksize
              - attributes
              - rdev:
                - major
                - minor
              - dev:
                - major
                - minor
    ...


Another possibility is to have a uint32 value instead of a sequence. In this,
case, it should correspond to a bitwise OR of the ``RBH_STATX`` macros defined
in ``librobinhood/include/robinhood/statx.h``.

Example:

.. code:: YAML

    --- !upsert
    id: !!binary aS1hbS1hLWZpbGUtaGFuZGxlCg==
    xattrs:
        rbh-fsevents:
            statx: 0x18000820 (e.g. RBH_STATX_ATIME | RBH_STATX_BTIME)

    ...


Note that upsert fsevents may already bear a ``struct statx`` which enrichers
are expected to use and merge with the more up-to-date information they collect.

.. note::

   A valuable though maybe complex optimization for the deduplicator would be to
   merge partial data of compatible fsevents such that only the smallest amount
   of information is ever queried out of the filesystem.


.. _null: https://yaml.org/type/null.html


``xattrs``
~~~~~~~~~~

This field is not currently used, but we assume it will be useful for things
like ``path`` with ``RBH_FET_LINK`` events.


``symlink``
~~~~~~~~~~~

This field should only appear on fsevents of type ``RBH_FET_UPSERT`` for file
entries of type ``S_IFLNK``.

If the field is present, enrichers should use readlinkat_ to get the content
of the symbolic link and fill the ``upsert.symlink`` field of the ``struct
fsevent``.


.. _readlinkat: https://man7.org/linux/man-pages/man2/readlink.2.html


Rejected Solutions
==================

Sentinels
---------

Using sentinels in ``struct rbh_fsevent`` to denote whether a given field is
undefined and should be filled in.

While this approach would likely lead to lower memory pressure, it is complex
to implement, and we could not figure out a portable way to store both defined
and undefined fields in the same ``struct statx``.
