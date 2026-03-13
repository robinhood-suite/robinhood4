rbh-gc(1)
=========

SYNOPSIS
--------

**rbh-gc** [**OPTIONS**] *BACKEND*

DESCRIPTION
-----------

Iterate on a RobinHood4 BACKEND's entries ready for garbage collection. If
these entries are absent from their original system, delete them from BACKEND
for good.

Mandatory arguments to long options are mandatory for short options too.

**BACKEND**
    Specify the backend to delete entries from. Should take the form of a
    RobinHood4 URI.

**-d, --dry-run**
    Display the list of absent entries.
    Print the generated command after alias substitution but does not execute
    it.

**-h, --help**
    Show the help message and exits.

**-s, --sync-time** *SYNC_TIME*
    Only consider for deletion entries with a sync_time lesser than `SYNC_TIME`.

**-v, --verbose**
    Display the request sent by the backend to the underlying storage system.

EXAMPLES
--------

Simple removal of deleted entries
    $ rbh-gc rbh:mongo:backend

Find deleted entries still present in the mirror backend
    $ rbh-gc rbh:mongo:backend --dry-run
    '/blob' needs to be deleted
    1 element total to delete

GC all entries with a sync time older than the current date - 10 seconds
    $ rbh-gc rbh:mongo:backend --sync-time $(( $(date +%s) - 10 ))

SEE ALSO
--------

robinhood4(5), robinhood4(7)

CREDITS
-------

rbh-gc and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
