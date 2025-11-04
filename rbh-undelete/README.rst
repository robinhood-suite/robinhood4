.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

############
rbh-undelete
############

rbh-undelete recovers deleted entries that have been previously archived.

Overview
========

rbh-undelete restores files deleted from a filesystem but whose content remains
archived in an HSM (Hierarchical Storage Management) backend.

Note: rbh-undelete can only recover deleted entries from a Lustre backend.

Usage
=====

The basic syntax for rbh-undelete is:

::

    rbh-undelete SOURCE DEST --restore

The **SOURCE** is a RobinHood URI pointing to the metadata mirror and the
**DEST** is a RobinHood URI pointing to the target entry to undelete.

Restore a deleted entry
========================

rbh-undelete can recreate a deleted entry that has been deleted and rebind it
to its old content. To enable this features, use the ``--restore`` option.

.. code-block:: bash

    rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/file --restore

Restore to an alternative location
===================================

rbh-undelete can recreate a deleted entry to a different path using the
``--output`` option.

.. code-block:: bash

    rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/file.txt \
        --restore --output /mnt/lustre/recovered_file.txt

List deleted entries
====================

rbh-undelete can display all the deleted entries under a specific directory by
using the ``--list`` option.

.. code-block:: bash

    rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre --list
