.. This file is part of RobinHood 4
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

###############
rbh-update-path
###############

rbh-update-path updates paths in one backend after directory renames.

Overview
========

rbh-update-path updates paths in the metadata mirror after directory renames.
This command allows you to keep file and directory paths consistent without
running another rbh-sync_.

When a directory is renamed, rbh-fsevents_ removes the directory path instead of
updating all paths in the directory tree to avoid slowing down rbh-fsevents_.
rbh-update-path complements it by updating the entire directory tree afterward.
It can be run in parallel with rbh-fsevents_, ensuring that all paths in the
mirror are up to date.

.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync

Usage
=====

The basic syntax for rbh-update-path is:

::

    rbh-update-path SOURCE

The SOURCE must be a RobinHood URI.

.. code:: bash

    rbh-update-path rbh:mongo:test

Example
=======

This example demonstrates how to use rbh-update-path:

Step-by-step
------------

Create a directory and a file inside it and synchronize it with rbh-sync_.

.. code-block:: bash

    mkdir dir
    touch dir/file

    rbh-sync rbh:lustre:. rbh:mongo:test

Rename the directory on the filesystem:

.. code-block:: bash

    mv dir new_dir

Run rbh-fsevents_ to update the mirror.

.. code-block:: bash

    rbh-fsevents -e rbh:lustre:. src:lustre:lustre-MDT0000 rbh:mongo:test

Check the filesystem:

.. code-block:: bash

    $ find . -printf "/%P\n"
    /
    /new_dir
    /new_dir/file

However, the metadata store still contains the old path entry
because rbh-fsevents_ removed the path for the directory but did not
rebuild the entire tree in the mirror:

.. code-block:: bash

    # rbh-find shows the stale path in the mirror
    $ rbh-find rbh:mongo:test
    /
    /dir/file

.. note::
    rbh-fsevents_ in this configuration removes the old path entry but
    does not reconstruct the metadata subtree for the renamed directory.
    Use rbh-update-path to fix the metadata tree.

Update the path tree:

.. code-block:: bash

    rbh-update-path rbh:mongo:test

Verify the mirror now matches the filesystem tree:

.. code-block:: bash

    $ rbh-find rbh:mongo:test
    /
    /new_dir
    /new_dir/file

.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
