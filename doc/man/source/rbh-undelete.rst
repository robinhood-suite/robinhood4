rbh-undelete(1)
===============

NAME
----

rbh-undelete - Restore deleted entries using archived metadata

SYNOPSIS
--------

**rbh-undelete** [**OPTIONS**] *SOURCE* *DEST*

DESCRIPTION
-----------

Restore the deleted entry DEST from its archived information available in
SOURCE. Do nothing if **--restore** or **--list** if not specified.

ARGUMENTS
---------

PARAMETERS
++++++++++

**SOURCE**
    Specify the metadata source where information about the entries to
    undelete should be fetched. Should take the form of an URI.

**DEST**
    Specify the entry to undelete and where it should be undeleted to (by
    default, the entry is recreated "in-place"). Should be an absolute path and
    take the form of an URI.

OPTIONS
-------

**-h, --help**
    Display the help message and exits.

**-c, --config** *PATH*
    Specify the configuration file to use.

**-l, --list**
    List all entries eligible for undelete.

**-r, --restore**
    Create a new entry corresponding to the deleted one and rebind its old
    content based on the archived information.

**--output** *OUTPUT*
    Specify the path where the entry to undelete should be recreated.
    This option must be used with **--restore**.

USAGE EXAMPLES
--------------

1. **List Deleted Entries**
   ```bash
   rbh-undelete rbh:mongo:blob rbh:lustre:/mnt/lustre/something --list
   ```

2. **Undelete an entry**
   ```bash
   rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/blob/foo/test.c --restore
   ```

3. **Undelete a file to a custom path**
   ```bash
   rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/blob/foo/test.c --restore --output /mnt/lustre/test.c
   ```

SEE ALSO
========

rbh4(5), rbh4(7)

CREDITS
=======

rbh-undelete and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
======

rbh-undelete and RobinHood4 is written by the storage software development team
of the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
