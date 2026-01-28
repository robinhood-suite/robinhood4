rbh-undelete(1)
===============

SYNOPSIS
--------

**rbh-undelete** [**OPTIONS**] *SOURCE* *DEST*

DESCRIPTION
-----------

Recreate entries that have been deleted from the filesystem using the metadata
from a Robinhood4 backend and the data from an external backup. Currently only
works with the Lustre filesystem.

Mandatory arguments to long options are mandatory for short options too.

**SOURCE**
    Specify the metadata source where information about the entries to
    undelete should be fetched. Should take the form of an URI.

**DEST**
    Specify the entry to undelete and where it should be undeleted to (by
    default, the entry is recreated "in-place"). Should be an absolute path and
    take the form of an URI.

**-c, --config** *PATH*
    Specify the configuration file to use.

**-h, --help**
    Display the help message and exits.

**-l, --list**
    List all entries eligible for undelete.

**-o, --output** *OUTPUT*
    Specify the path where the entry to undelete should be recreated.
    This option must be used with **--restore**.

**-r, --restore**
    Create a new entry corresponding to the deleted one and rebind its old
    content based on the archived information.

EXAMPLES
--------

List Deleted Entries
    | $ rbh-undelete rbh:mongo:blob rbh:lustre:/mnt/lustre/something --list
    | DELETED FILES:
    | -- rm_path: /tmp.OCsulC9d3r/test_list-test_list/test   rm_time: Mon Dec 15 14:51:41 2025

Undelete an entry
    | $ rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/blob/foo/test.c --restore
    | '/mnt/lustre/blob/foor/test.c' has been undeleted, new FID is '[0x200000404:0xd:0x0]'

Undelete a file to a custom path
    | $ rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/blob/foo/test.c --restore --output /mnt/lustre/test.c
    | '/mnt/lustre/test.c' has been undeleted, new FID is '[0x200000404:0x1a:0x0]'

SEE ALSO
--------

robinhood4(5), robinhood4(7)

CREDITS
-------

rbh-undelete and RobinHood4 is written by the storage software development team
of the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
