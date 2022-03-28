.. This file is part of rbh-find-lustre
   Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

########
rbh-find-lustre
########

rbh-find-lustre is an overload of `rbh-find(1)`__.

.. __: rbh-find_
.. _rbh-find: https://github.com/cea-hpc/rbh-find.git

Installation
============

Install the `RobinHood library`_ and the `rbh-find library`_ first, then
download the sources:

.. code:: bash

    git clone https://github.com/cea-hpc/rbh-find-lustre.git
    cd rbh-find-lustre

Build and install with meson_ and ninja_:

.. code:: bash

    meson builddir
    ninja -C builddir
    sudo ninja -C builddir install

.. _meson: https://mesonbuild.com
.. _ninja: https://ninja-build.org
.. _RobinHood library: https://github.com/cea-hpc/librobinhood
.. _rbh-find library: https://github.com/cea-hpc/rbh-find

A work in progress
==================

rbh-find-lustre is a close twin of `rbh-find(1)`__. It focuses more on the use
of the Lustre backend of the `RobinHood library`_. As such, it will implement
some Lustre specific features, but will still behave exactly like `rbh-find`.

.. __: _rbh-find

Usage
=====

`rbh-lfind` works so much like find that documenting how it works would look
a lot like `rbh-find's documentation`_. Instead, we will document the
differences of `rbh-lfind` compared to `rbh-find`.

.. _: https://github.com/cea-hpc/rbh-find/README.rst

Examples
--------

WORK IN PROGRESS
