.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#########
RobinHood
#########

This is the 4th version of RobinHood. This version has a more modular approach
to development. Although the core utilities are grouped in the same repository,
they are built on a core library librobinhood__. Any extension can be added to
this tool suite through the core library.

.. __: https://github.com/robinhood-suite/robinhood4/librobinhood/blob/main/README.rst

This repository contains the core components necessary to use robinhood
alongside Lustre_ or any POSIX file system.

The main components of RobinHood are:
 - librobinhood_ the core API to interact with backends
 - rbh-sync_ to synchronize two backends
 - rbh-fsevents_ to update a backend with changelog events
 - rbh-find_ to query a backend and filter entries
 - rbh-lfind_ an overload of rbh-find specific to Lustre

.. _librobinhood: https://github.com/robinhood-suite/robinhood4/tree/main/librobinhood
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-lfind: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find-lustre
.. _rbh-info: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-info
.. _Lustre: https://lustre.org

Installation
============

Download the sources:

.. code:: bash

    git clone https://github.com/robinhood-suite/robinhood4
    cd robinhood4

RobinHood supports various backends that can be optionally enabled.
To change the defaults, you need to update the file `meson_options.txt`.
It supports 3 options:

 - `lustre`: enabled by default, build the `lustre` backend and
   the `rbh-find-lustre` command;
 - `mfu`: enabled by default, build `posix-mpi`, `lustre-mpi` and `mpi-file`
   backends. Both `lustre` and `mfu` must be enabled to build the `lustre-mpi`
   backend;
 - `hestia`: disabled by default, build the `hestia` backend;

Build and install with meson_ and ninja_:

.. code:: bash

    meson builddir
    ninja -C builddir
    sudo ninja -C builddir install

.. _meson: https://mesonbuild.com
.. _ninja: https://ninja-build.org

You can also generate RPMs. For this you will need the `rpmbuild` command.

.. code:: bash

    cd builddir
    meson compile rpm

Documentation
=============

Refer to each component's README.rst file for more information.
