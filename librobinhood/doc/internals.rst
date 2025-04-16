.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#########
RobinHood
#########

The RobinHood project is made of a library_ and several applications_. Its main
goal is to provide efficient and easy to use tools to store and query any
filesystem's metadata.

Context
=======

POSIX filesystems have limited metadata querying interfaces. To answer questions
such as: "what are the 10 biggest files on this FS?", or "what is the last
modified file on that FS?", one must traverse the whole filesystem and build the
answer iteratively.

For network filesystems, filesystems on tapes, very large filesystems, or simply
filesystems running on slow hardware, the cost of a full traversal can be
prohibitive; and the load incurred can disrupt other processes that need to
access the filesystem as well.

RobinHood answers both of those issues by maintaining a mirror of a filesystem's
metadata in a queryable backend. The exact storage technology used as a
RobinHood backend is hidden behind a layer of abstraction [#]_ but it is usually
a database. This allows more expressive queries, and better performance overall.

.. [#] to make sure RobinHood does not need to be rewritten from scratch if
       better backends become available.

Library
=======

The library represents the core of the project. This is where most of the
project's complexity is handled. It provides several abstractions (or
interfaces). The four most important, and specific to RobinHood, are:

* Backend_
* FSEntry_
* Filter_
* FSEvent_

RobinHood also provides a few others, whose names should suffice to infer their
purpose:

* Iterators_
* Plugin_
* URI_

And some more, for basic data structures, that are mostly relevant to the
project's developers: [#]_

* Stack_
* Ring_
* Sstack_
* Queue_

.. [#] applications may choose to depend on them, they are considered part of
       RobinHood's API

Each abstraction is implemented as a (potentially opaque) data structure and a
set of functions.

Backend
-------

.. _backends: backend_

Abstraction over a storage medium/technology. Acts as a mirror of a filesystem's
metadata.

It exposes three main operations: one to update the metadata it manages, another
to query it, and a third one we will describe later. The updating part is done
by providing a stream of fsevents_ to the backend. The querying part works by
providing a filter_ to the backend which then returns a stream of
fsentries_ matching said filter. The filter may be ``NULL``, in which case the
backend simply returns every fsentry it knows of.

As RobinHood is not directly involved in filesystems' metadata update process,
backends may contain and report stale information. Besides, current filesystems'
interfaces do not provide enough information about metadata updates to implement
strongly coherent backends. Backends can thus temporarily return fsentries_ in
states that never occured. [#]_

The official tool provided by RobinHood to update backends [#]_, if used
correctly, ensures backends are **eventually consistent** mirrors of a
filesystem's metadata.

The third backend operation is named "branching". Directories and files of a
filesystem can be seen as the nodes and leaves of a tree. In RobinHood's
semantics, branching means creating a backend from another backend by selecting
only a subtree (or a "branch") of the original.

Currently, RobinHood implements three backends: the `POSIX backend`_,
`MongoDB backend`_ and the `Lustre backend`_.

.. [#] this may sound more dramatic than it is. In practice, the same thing
       can happen when you call statx_. RobinHood simply operates at greater
       latencies and so inconsistencies are more likely to be noticeable.

.. _statx: http://man7.org/linux/man-pages/man2/statx.2.html

.. [#] rbh-sync_

POSIX backend
~~~~~~~~~~~~~

This backend implements only the branching and the querying part of the
interface. And even that last part is only partially implemented: filtering is
not supported (any filter_ other than ``NULL`` will yield an ``ENOTSUP`` error).

It may seem odd, but this backend is just a means to an end. Its purpose is to
allow dumping any POSIX filesystem's metadata in a format RobinHood can easily
reuse (ie. fsentries_).

This is used by rbh-sync_ to perform a scan of a filesystem's metadata and
update a backend with it.

MongoDB backend
~~~~~~~~~~~~~~~

This is the first backend fully supported by RobinHood. It stores its data in
a Mongo database. It is meant to be both fast and scalable.

As the first fully functional backend implemented by RobinHood, it had quite an
impact on the design of the whole project. For example, MongoDB does not store
graphs very well [#]_. That in turn discouraged the implementation of features
such as filtering entries from backends based on their depth in the filesystem.

Refer to MongoDB's website__ for more information about what a Mongo database is
and how it works.

.. [#] at the time of writing, Mongodb supported traversing documents much like
       a graph, but only on a single node.

__ https://docs.mongodb.com/manual/

Lustre backend
~~~~~~~~~~~~~~~

This backend is an extension of the POSIX backend, in that it will only overload
the extended attributes retrieval, to enrich the entries with Lustre specific
values. Otherwise, it behaves exactly like the POSIX backend, provides the
same features and fills the same goals.

The retrieved extended attributes are the following:
 - fid
 - hsm state and archive ID
 - layout main flags
 - if the file is a regular file, the magic number and layout generation
 - if the file has a composite layout, the mirror count
 - per component of the file:
   - stripe count and size
   - pattern
   - flags
   - pool
   - ost
   - if the file is composite, the mirror ID, and start and end offsets of each
   component

Refer to Lustre's documentation__ for more information about what these
attributes correspond to.

__ https://wiki.lustre.org/Main_Page

The Lustre backend also supports retention attributes. To do so, it will
interpret the regular xattr ``user.expires``. This attribute can be changed by
providing a configuration file to the used application, and setting the
`RBH_RETENTION_XATTR` key.

This attribute must have a value corresponding to either an epoch or a number
of seconds preceded by a plus sign.

If the value is solely an epoch, it will be considered as the expiration date
of the file, regardless of when it was last accessed.

If the value is a number preceded by a plus sign, that number will be
compared to the maximum between the file's access time and modification time.
This means that the file will expire when **max(atime, mtime) + number <=
queried_epoch**. This calculation is only performed when synchronizing, and not
when querying expired files.

**The retention support is in the Lustre backend for the time being, but will
soon be its own backend.**

FSEntry
-------

.. _fsentries: FSEntry_

This is RobinHood's representation of a filesystem entry and the metadata
associated with it.

It is a structure that represents the most common metadata attributes (name,
size, owner, ...) of an entry (file, directory, symlink, ...) in a POSIX or
Lustre filesystem. An fsentry is also able to hold structured extended
attributes.

Each fsentry is uniquely identified by an ID.

Filter
------

A filter represents a set of criteria relating to metadata attributes. It is
used to fetch a limited number of fsentries_ from a backend_.

Filters are designed to be as expressive as possible while still allowing
backends_ to translate them into native queries for the storage technology they
abstract.

In pratice, the design was largely driven by the needs of rbh-find_ and the
limitations of the `MongoDB backend`_.

FSEvent
-------

.. _fsevents: FSEvent_

A structure that describes a metadata change. An fsevent can be applied to a
backend_ to create, update, or delete an fsentry_.

RobinHood distinguishes two types of fsevents: those that modify inode
attributes, and those that modify the namespace. For example, renaming a file
modifies both the namespace (the file is renamed and/or moved to a new
directory), and the underlying inode (its ctime is updated).

Ideally, given a series of fsevents, any permutation of that series could be
applied to a backend_ and still yield the same result. Unfortunately, this is
not the case. [#]_ RobinHood requires that fsevents that modify the same field
of the same fsentry_ be applied in the order they happened on the filesystem.

So the order in which fsevents are applied on a backend matters. One interesting
property of fsevents though, is that any sequence may be replayed, any number of
times, and still converge to the same end state.

.. [#] whether or not this is theoretically possible is left to the reader to
       figure out (and share with the community if they reach a conclusion).

Iterators
---------

A classical iterator interface.

RobinHood implements two types of iterators: iterators over constant (ie.
read-only) references, and iterators over mutable (ie. regular) references. This
distinction allows for a clear delegation of memory ownership at the API level:
constant references must not be freed [#]_ nor modified; mutable references may
be modified and must be freed [#]_.

The most important operation of iterators is their ``next()`` method, which
yields the next reference the iterator contains.

Much like Python's itertools_ module, RobinHood provides helpers to create,
transform, and combine iterators.

.. [#] except maybe in very specific cases.

.. [#] except maybe in some cases.

.. _itertools: https://docs.python.org/3/library/itertools.html

Plugin
------

The plugin interface defines how external libraries should be named, and which
structure they should expose in order to be easily imported and used by
RobinHood applications. This allows choosing between different implementations
of the same interface at runtime.

This is particularly useful for backends_. There are many candidate
technologies, and the RobinHood development team cannot be expected to support
them all. Making backends pluggable allows for anyone to implement their
favourite storage technology as a backend and use RobinHood tools out of the
box.

The plugin interface itself is a bit too generic to be useful: it only exposes
a structure with two fields (``name`` and ``version``) and a method to import
such a symbol from a dynamic library. Fortunately, it is easily extended to
support more operations.

Backend plugin
~~~~~~~~~~~~~~

This interface builds upon the plugin interface to define how to instantiate
a backend implemented in a dynamic library.

There are many candidate technologies over which one can implement the backend_
interface. The RobinHood development team cannot be expected to support them
all. The backend plugin interface solves this by allowing anyone to implement
support for their favourite storage technology and have RobinHood applications
use them just as well as any officially supported backend.

URI
---

URIs are the preferred method to designate robinhood resources, be it backends_
or fsentries_. As defined by [RFC3986_], a URI looks like this::

    scheme:[//authority]path[?query][#fragment]

Where each part of a URI has a generic meaning which can be refined by the
``scheme``.

RobinHood uses its own scheme: ``rbh``. [#]_ It makes no use of the
``authority`` or ``query`` components. [#]_ The ``path`` component is made of
a ``backend-type`` and an ``fsname`` separated by a colon (":"). Importing the
"pchar", "unreserved", "pct-encoded" and "sub-delims" rules from [RFC3986_].

::

    path         = backend-type ":" fsname
    backend-type = 1*(pchar-nc / "/")
    fsname       = 1*(pchar / "/")

    pchar-nc     = unreserved / pct-encoded / sub-delims / "@"

    ; the following rules are defined in RFC3986 and summarized in appendix A
    ; of the same document

    pchar        = unreserved / pct-encoded / sub-delims / ":" / "@"
    unreserved   = ALPHA / DIGIT / "-" / "." / "_" / "~"
    pct-encoded  = "%" HEXDIG HEXDIG
    sub-delims   = "!" / "$" / "&" / "'" / "(" / ")"
                 / "*" / "+" / "," / ";" / "="

``backend-type`` identifies a type of backend. The officially supported values
are:

* ``posix`` for the `Posix Backend`_;
* ``mongo`` for the `MongoDB Backend`_.
* ``lustre`` for the `Lustre Backend`_.

Given ``backend-type``, ``fsname`` uniquely identifies an instance of that type
of backend. The format and further meaning attached to ``fsname`` depend on the
value of ``backend-type``. For example, for:

* ``posix``, ``fsname`` is the root of the filesystem used;
* ``mongo``, ``fsname`` is the name of the database used.
* ``lustre``, ``fsname`` is the root of the filesystem used;

Unofficial `backend plugin`_ implementations are welcome to choose a name for
themselves, and attach their own meaning to ``fsname``. The RobinHood project
rejects any responsibility if a conflict on that matter should arise.

The ``fragment`` component can either be a filepath, relative to the root of the
backend which the URI designates; or a square bracket enclosed fsentry ID. When
the fragment is a path, only the first character, if it is an opening square
bracket, need to be percent-encoded. When applicable, the fsentry ID might be
replaced with a Lustre File IDentifier (FID), in which case, the three colons
(":") in the FID must not be percent-encoded.

::

    fragment     = filepath / "[" (fsentry-id / lustre-fid) "]"
    filepath     = *(pchar / "/")
    fsentry-id   = *(pchar-nc / "/" / "?")
    lustre-fid   = num ":" num ":" num

    num          = decnum / hexnum
    decnum       = *(DIGIT)
    hexnum       = "0x" 1*(HEXDIG)

A RobinHood URI without a ``fragment`` component represents either a backend
instance, or all the fsentries managed by that instance. With a ``fragment``
component, a RobinHood URI refers to a particular fsentry in a particular
backend instance, and sometimes, when the fsentry at stake is a directory, the
URI may refer to that fsentry and all the fsentries under it.

Here are a few examples of valid RobinHood URIs::

    rbh:mongo:test
    rbh:posix:/mnt
    rbh:lustre:/mnt/lustre
    rbh:mongo:scratch#test-user/dir0
    rbh:my-backend:store#[0x0:0x1:0x2]

.. _RFC3986: https://tools.ietf.org/html/rfc3986
.. [#] which it should reserve with IANA__. Soon.
.. __: https://www.iana.org/assignments/uri-schemes/uri-schemes.xhtml
.. [#] at least for now.
.. _ABNF: https://tools.ietf.org/html/rfc5234

Stack
-----

A regular fixed-size stack.

A stack has a fixed size, meaning there is a maximum number of bytes that can be
pushed into it; there is no additional restriction on the number of bytes that
can be pushed at once.

Ring
----

A regular fixed-size ring buffer.

A ring has a fixed size, meaning there is a maximum number of bytes that can be
pushed into it; there is no additional restriction on the number of bytes that
can be pushed at once.

Sstack
------

A dynamically growing stack.

An sstack has an unlimited capacity [#]_ but one can only push a limited number
of bytes into it at once. That number is configurable at creation time.

.. [#] capped by the amount of memory available on the system.

Queue
-----

A dynamically growing queue.

A queue has an unlimited capacity [#]_ but one can only push a limited number
of bytes into it at once. That number is configurable at creation time.

.. [#] capped by the amount of memory available on the system.

Applications
============

The RobinHood project includes several tools:

* rbh-sync_
* rbh-fsevents_
* rbh-find_

rbh-sync
--------

This tool allows synchronizing any two RobinHood backends, provided the source
backend implements the querying part of the backend_ interface, and the
destination backend implements the updating part.

This is the tool of choice to keep backends in sync with filesystems that do not
support a log of metadata changes. Refer to the project's documentation__ for
more information.

__ https://github.com/cea-hpc/rbh-sync

rbh-fsevents
------------

This tool allows updating a RobinHood backend using changelog events from a
source similar to the backend (for instance, Lustre changelogs to update a
Lustre mirror). The destination backend must implement the updating part of the
backend_ interface.

This is the tool of choice to keep a backend sync-ed after an initial call to
rbh-sync_ when the backend support a log of metadata changes. Refer to the
project's documentation__ for more information.

__ https://github.com/cea-hpc/rbh-fsevents

rbh-find
--------

Basically a clone of `(gnu-)find`_. Refer to the project's documentation__ for
more information.

.. _(gnu-)find: https://www.gnu.org/software/findutils/
__ https://github.com/cea-hpc/rbh-find
