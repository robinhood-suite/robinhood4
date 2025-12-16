rbh-update-path(1)
==================

NAME
----

rbh-update-path - update the path of directory trees in SOURCE after directories
rename

SYNOPSIS
--------

**rbh-update-path** [**OPTIONS**] *SOURCE*

DESCRIPTION
-----------

Update the path of directory trees in the metadata mirror after directories
have been renamed on the filesystem. rbh-update-path walks the metadata tree
for the specified SOURCE and rebuilds or corrects path entries that were
removed or left stale by directory rename operations from rbh-fsevents. This
allows the metadata mirror to reflect the filesystem hierarchy without
performing a full resynchronization with rbh-sync.

This command is intended to complement rbh-fsevents, which may remove old
directory path entries instead of rebuilding whole subtrees in order to stay
fast. rbh-update-path can be run in parallel with rbh-fsevents to ensure the
mirror paths are brought back to a consistent state after renames.

ARGUMENTS
---------

PARAMETERS
++++++++++

**SOURCE**
    Specify the backend containing the metadata mirror to update. Should take
    the form of a RobinHood4 URI.

OPTIONS
+++++++

**-h, --help**
    Display the help message and exits.

**-c, --config** *PATH*
    The configuration file to use, if not specified, will use
    `/etc/robinhood4.d/default.yaml`.

EXAMPLES
--------

**Basic usage: update path entries in a mirror**:
.. code:: bash

   rbh-update-path rbh:mongo:test

Example — step by step
++++++++++++++++++++++

1. **Create a directory and a file and synchronize it with rbh-sync**:
.. code:: bash

    mkdir dir

    touch dir/file

    rbh-sync rbh:lustre:. rbh:mongo:test

2. **Rename the directory on the filesystem**:
.. code:: bash

    mv dir new_dir

3. **Run rbh-fsevents to notify the mirror**:
.. code:: bash

    rbh-fsevents -e rbh:lustre:. src:lustre:lustre-MDT0000 rbh:mongo:test

At this point, the filesystem looks like:
.. code:: bash

    find . -printf "/%P\n"

    /

    /new_dir

    /new_dir/file

but the metadata mirror may still show the stale path:
.. code:: bash

    rbh-find rbh:mongo:test

    /

    /dir/file

4. **Fix the mirror by updating the path tree**:
.. code:: bash

    rbh-update-path rbh:mongo:test

5. **Verify the mirror now matches the filesystem**:
.. code:: bash

    rbh-find rbh:mongo:test

    /

    /new_dir

    /new_dir/file

SEE ALSO
--------
robinhood4(5), robinhood4(7)

CREDITS
-------

rbh-update-path and RobinHood4 are distributed under the GNU Lesser General
Public License v3.0 or later. See the file COPYING for details.

AUTHOR
------

rbh-update-path and RobinHood4 is written by the storage software development
team of the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
