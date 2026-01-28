RobinHood4(7)
=============

URI
===

RobinHood4 tools use Universal Resource Identifiers (URI,
https://datatracker.ietf.org/doc/html/rfc3986) to target resources.
More specifically, they target backend from which to fetch or update metadata,
and are built as follows:
    rbh:BACKEND:FSNAME[#BRANCH]

Where:
    | BACKEND       is the name of a backend (see below for more information)
    | MOUNTPOINT    is an instance of BACKEND (e.g. path for filesystems, database name for databases, ...).
    | BRANCH        is the path/id of an fsentry managed by BACKEND:FSNAME. IDs must be enclosed in square brackets '[ID]' to distinguish them from a path. Currently only Lustre FIDs are handled.

BACKEND
-------

By default, the BACKEND must correspond to a plugin currently installed. This
list can be checked with rbh-info(1), as such:
    $ rbh-info --list

These plugins act as ways to interact with a storage system, either by fetching
metadata from it, or by updating data in it.

In any case, they define what metadata should be retrieved, for instance the
size of a file or the bucket an object belongs to, or how those metadata should
be inserted, for instance the schema of a database.

While using only plugins is perfectly doable, there are extensions available
for different plugins, which extend the information fetched or to update. To
use them, they must be defined as additional backends in the configuration file.
Check robinhood4(5) for more information.

MOUNTPOINT
----------

All entries under a mount point will be examined, and every path, name or
plugin-related information will be based around that mount point. For instance,
if you have a file test in the /tmp directory, and synchronize that directory,
the entry test in your mirror system will have its path recorded as /test, and
not /tmp/test. Moreover, the mount point itself will be recorded, with a name
being an empty string and the path being /.

This allows RobinHood4 to work in a storage-system-agnostic fashion, as for
instance, if you work with network filesystems, and synchronize your home on
one node, then the resulting mirror will still be usable on another node, as
long as your home is also available there.

BRANCH
------

A branch in RobinHood4 allows one to target specific subset in the given
storage system. For instance, in a POSIX architecture, you can add a branch to
target only subdirectories in the main path, like so:
    $ rbh:posix:/tmp#somewhere/something

This is important because it allows one to still target the same system (here,
`/tmp`) without targetting all of its content when it isn't necessary.

For ID targeting, only Lustre FIDs are handled, and here is an example in a
Mongo database:
    $ rbh:mongo:blob#[0x200000404:0x1:0x0]

ADDITIONAL PARAMETERS
---------------------

The URI is parsed as an actuel URI, meaning that every component of a URI is
correctly parsed and understood by RobinHood4. This however does not mean the
targeted backend is able to understand these additional parameters.

For instance, `rbh://test:42@localhost:27018/lustre:/mnt/lustre?something="42"`
is a valid URI, but the Lustre backend won't use any additional information
specified in it.

Moreover, these parameters are also tool dependant. For instance, with the URI
`rbh:lustre:/mnt/lustre?ack-user=cl1`, the `ack-user` part won't matter to
rbh-sync(1), as it doesn't understand it, but will be important to
rbh-fsevents(1).

EXAMPLES
--------

| rbh:posix:/tmp
| rbh:lustre:/mnt/lustre/
| rbh:mongo:blob
| rbh://208.80.152.201:27018/mongo:blob
| rbh:mpi-file:dummy.mpi

SEE ALSO
--------

rbh-info(1), rbh-find(1), rbh-report(1), rbh-sync(1), rbh-fsevents(1), rbh-undelete(1), robinhood4(5)

CREDITS
-------

RobinHood4 is written by the storage software development team of the
Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
