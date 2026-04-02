.. This file is part of RobinHood
   Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

################
Sparse extension
################

The Sparse extension is an enricher for the POSIX plugin.
It is comprised of two parts: the information enrichment and the information
filtering and printing.

The first part is mainly used by rbh-sync_ and rbh-fsevents_.
The second part is used by rbh-find_ and rbh-report_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

Enrichment
==========

Here are the additional information enrichment for the POSIX plugin:
 * For regular files:
   * Sparseness in percentage, calculated as (512 * st_blocks / st_size) * 100

Filtering
=========

The Sparse extension defines predicates and directives that will be used with
rbh-find_ and rbh-report_. The predicates define filters that will be usable by
users to filter data retrieved from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-report: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-report

Below are defined the predicates defined by the Retention extension.

-sparse
-------

A predicate that filters entries which are considered sparse, i.e. which have
a sparseness inferior to 100.

.. code:: bash

    rbh-find rbh:mongo:test -sparse
    ./dir/sparse-file

Printing
========

The Sparse extension defines directives that will be used with rbh-find_.
They correspond to which data is printed when using the `-printf` and
`-fprintf` actions of rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Below are defined the directives allowed by the Sparse extension.

+-------------+--------------------------------------------+
|  Directive  | Information printed                        |
+=============+============================================+
|     '%S'    | The file's sparseness, printed between     |
|             | 0 and 1.                                   |
+-------------+--------------------------------------------+
