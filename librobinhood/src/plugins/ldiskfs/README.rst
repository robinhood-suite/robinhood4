.. This file is part of RobinHood
   Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

##############
ldiskfs plugin
##############

The ldiskfs plugin is a plugin that will retrieve information from ldiskfs filesystems.
It mainly gathers informations contained in the inodes of the filesystem, but can also retrieve the Lustre layout of a file if the Lustre enricher of the POSIX backend is built.

This backend is mainly meant to be used by rbh-sync_ and rbh-find_.

.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync

To call this backend, you must specify the path to the block device you wish to scan in the URI.
For example:
.. code:: bash
    rbh-find rbh:ldiskfs:/dev/mapper/mds1_flakey


This command will use the ldiskfs backend to scan the filesystem located in the block device found at `/dev/mapper/mds1_flakey`

