.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

################
MPI-File plugin
################

The MPI-File plugin is a plugin that will retrieve the information contains
inside a mpiFileUtils__ file. The information contained in a mfu file are a
subset of the POSIX information.
It mainly iterates through a mfu file using MPI.

.. _mpifileutils: https://mpifileutils.readthedocs.io/

The iteration/enrichment part is mainly used by rbh-sync_.
The predicates part is mainly used by rbh-find_.

.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Enrichment
==========

Here are the additional information enrichment for the MPI-File plugin:
 * For all entries:
   * statx information:
     * access time, change time, modify time:
       * seconds
       * nanoseconds
     * group ID
     * mode and type
     * size
     * user ID
   * name
   * relative path from the root
 * For symbolic links:
   * link's target
 * For directories:
   * number of children

Filtering
=========

The MPI-File plugin defines predicates that will be used with rbh-find_. The
predicates define filters that will be usable by users to filter data retrieved
from a backend.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

The following predicates are supported by the MPI-File plugin:
 * -[acm]min
 * -[acm]time
 * -name
 * -path
 * -size
 * -type

For detailed information about these predicates and their usage, please refer
to the POSIX plugin README or find(1)__ manual page.
.. _: find
.. _find: http://man7.org/linux/man-pages/man1/find.1.html
