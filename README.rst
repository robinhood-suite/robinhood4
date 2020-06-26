.. This file is part of rbh-sync
   Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

########
rbh-sync
########

rbh-sync allows synchronizing one backend with another.

Installation
============

Install the `RobinHood library`_ first, then download the sources:

.. code:: bash

    git clone https://github.com/cea-hpc/rbh-sync.git
    cd rbh-sync

Build and install with meson_ and ninja_:

.. code:: bash

    meson builddir
    ninja -C builddir
    sudo ninja -C builddir install

.. _meson: https://mesonbuild.com
.. _ninja: https://ninja-build.org
.. _RobinHood library: https://github.com/cea-hpc/robinhood/tree/v4

Usage
=====

rbh-sync can be used to create or synchronize one `RobinHood Backend`_ [#]_ with
another.

.. [#] simply put, a storage backend which RobinHood can use to store a
       filesystem's metadata, and later query it.

.. _RobinHood Backend:
       https://github.com/cea-hpc/robinhood/tree/v4/doc/robinhood.rst#backend

RobinHood URIs
--------------

RobinHood uses URIs_ to identify backends. It uses its own scheme. The syntax
for the RobinHood scheme is detailed in the `library's documentation`__. Here
are the key takeaways:

* RobinHood URIs always start with ``rbh:``;
* followed by a type of backend (currently either ``mongo`` or ``posix``); [#]_
* another colon (":");
* and a filesystem identifier, which we call an ``fsname``.

::

    the scheme       the filesystem's identifier
           vvv       vvvvvvvvv
           rbh:mongo:my-fsname
               ^^^^^
         the backend type

This references a (whole) backend.

The syntax for ``fsname`` depends on the backend's type:

* for ``mongo`` it can pretty much be anything you want; [#]_
* for ``posix`` it must be the path to the backend's root.

::

    rbh:posix:/mnt/path/to/dir
    rbh:mongo:something-that-makes-me-think-of-/math/path/to/dir
    rbh:posix:/scratch
    rbh:mongo:scratch

Optionnally, you can append a ``#``, followed by either:

* a path (relative to the backend's root);
* or an fsentry_'s ID enclosed in brackets ("[", "]"), or a FID if the backend
  manages entries of a Lustre filesystem.

This references a particular fsentry in a backend.

::

    rbh:posix:/scratch#testuser/somedir
    rbh:mongo:scratch#[0x200000007:0x1:0x0]

We choose not to put an example with a regular fsentry's ID here, as they are
impractical to write on a command line.

The interested user should know that to use this syntax, they will need to
percent-escape any reserved or non-printable character. Refer to RFC3986's
sections 2.1_ and 2.2_ for more information on this.

.. [#] this is used by applications to infer which dynamic lybrary should be
       used to interact with the backend.
.. [#] it will be used as the name of the actual Mongo_ database.

.. _URIs: RFC3986_
.. _RFC3986: https://tools.ietf.org/html/rfc3986
.. _2.1: https://tools.ietf.org/html/rfc3986#section-2.1
.. _2.2: https://tools.ietf.org/html/rfc3986#section-2.2
.. __: https://github.com/cea-hpc/robinhood/tree/v4/doc/robinhood.rst
.. _fsentry: https://github.com/cea-hpc/robinhood/tree/v4/doc/robinhood.rst#fsentry
.. _Mongo: https://www.mongodb.com

CLI
---

What is all the fuss about RobinHood URIs then? Well they are integral to
rbh-sync's command line interface. [#]_

Much like rsync, rbh-sync takes two arguments, both of which are URIs::

    rbh-sync rbh:posix:/mnt/scratch rbh:mongo:scratch

This synchronizes ``rbh:mongo:scratch`` with ``rbh:posix:/mnt/scratch``, meaning
that when the process terminates, ``rbh:mongo:scratch`` should contain a copy
of all the metadata in ``rbh:posix:/mnt/scratch`` when the process started.

.. [#] and likely any other RobinHood application.

Consistency
-----------

An important thing to remember is that rbh-sync does not freeze the source
backend nor the destination backend. Thus, if they are modified at the same time
rbh-sync uses them, rbh-sync cannot garantee that it will do the right thing.

For example, if the source backend is updated while rbh-sync uses it, rbh-sync
might:

#. miss the update;
#. see an incomplete version of the update;
#. see the update entirely.

In both the first and second cases, the destination backend will contain stale
metadata at the end of the run.

Conversely, if the destination backend is updated while rbh-sync operates on it,
there is not particular garantee that the resulting metadata will be consistent.

To work around this, if either the source backend or the destination backend
was updated while rbh-sync ran, just re-run rbh-sync again.

The destination backend might never be exactly up-to-date, but you can be sure
that it will always go *forward*. In this sense, you get a level of consistency
comparable to that of a local filesystem: `eventual consistency`__.

.. __: https://en.wikipedia.org/wiki/Eventual_consistency

Parallelism
-----------

rbh-sync is fundamentally a single-threaded program. There is no plan to
parallelize it any time in the future.

Nevertheless, rbh-sync being a single-threaded program does not mean you cannot
run several instances of it, in parallel. The following script should therefore
provide a reasonable amount of parallelization, without sacrificing consistency.

.. code:: bash

    for entry in /path/to/dir/*; do
        rbh-sync rbh:posix:/scratch#"$entry" rbh:mongo:scratch &
    done
    rbh-sync --one rbh:posix:/scratch rbh:mongo:scratch &
    wait

*Note that the* ``--one`` *option is not currently implemented, which means that
you will need to skip that step for now. Your backend will be missing metadata
about the* ``/scratch`` *directory, but this probably won't be much of a
problem. This is* **temporary**.

Also, since rbh-sync heavily relies on the backends' implementation, if these
were to implement any sort of parallelization, rbh-sync would transparently
benefit from it.
