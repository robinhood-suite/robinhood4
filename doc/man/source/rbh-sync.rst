rbh-sync(1)
===========

NAME
----

rbh-sync - synchronize metadata from one backend to another

SYNOPSIS
--------

**rbh-sync** [**PRE-OPTIONS**] *SOURCE* *DEST* [**POST-OPTIONS**]

DESCRIPTION
-----------

Synchronize metadata from SOURCE to DEST. rbh-sync will iterate through every
entry in SOURCE, and for each one, will copy its metadata to DEST. The
metadata retrieved depends on the type of SOURCE being used. Both SOURCE and
DEST must take the form of URIs.

Mandatory arguments to long options are mandatory for short options too.

ARGUMENTS
---------

PARAMETERS
++++++++++

**SOURCE**
    Specify the backend to use and which underlying storage system should be
    synchronized. Should take the form of an URI.

**DEST**
    Specify where the metadata from SOURCE should be synchronized to. Should
    take the form of an URI.


PRE-OPTIONS
+++++++++++

**-h, --help**
    Display the help message and exits.

**-c, --config** *PATH*
    The configuration file to use, if not specified, will use
    `/etc/robinhood4.d/default.yaml`.

POST-OPTIONS
++++++++++++

**-d, --dry-run**
    Displays the command after replacing every alias in it, and stop the
    command.

**-f, --field** *[+-]FIELD*
    Select, add or remove a FIELD to synchronize (can be specified multiple
    times)

    FIELD can be any of the following:
        [x] id          [x] parent-id   [x] name        [x] statx
        [x] symlink     [x] ns-xattrs   [x] xattrs

    Where 'statx' also supports the following subfields:\n"
        [x] blksize     [x] attributes  [x] nlink       [x] uid
        [x] gid         [x] type        [x] mode        [x] ino
        [x] size        [x] blocks      [x] atime.nsec  [x] atime.sec
        [x] btime.nsec  [x] btime.sec   [x] ctime.nsec  [x] ctime.sec
        [x] mtime.nsec  [x] mtime.sec   [x] rdev.major  [x] rdev.minor
        [x] dev.major   [x] dev.minor   [ ] mount-id

    [x] indicates the field is included by default
    [ ] indicates the field is excluded by default

**-n, --no-skip**
    Do not skip errors when synchronizing metadata. By default, if an
    entry-related error occurs during rbh-sync's run, it is skipped.

**-o, --one**
    Only consider the root of SOURCE and do not synchronize anything else.

EXAMPLES
--------

1. **Synchronize a POSIX hierarchy to a Mongo database**:
   ```bash
   rbh-sync rbh:posix:/etc rbh:mongo:blob
   ```

2. **Synchronize a POSIX hierarchy to a distant Mongo database**:
   ```bash
   rbh-sync rbh:posix:/etc rbh://208.80.152.201:27018/mongo:blob
   ```

3. **Synchronize a POSIX hierarchy to a Mongo database without the size and type of each entry**:
   ```bash
   rbh-sync rbh:posix:/etc rbh:mongo:blob --field -statx.size --field -statx.type
   ```

4. **Synchronize a POSIX hierarchy to a SQLite database**:
   ```bash
   rbh-sync rbh:posix:/etc rbh:sqlite:blob.sqlite
   ```

5. **Synchronize only a Lustre mountpoint to a Mongo database**:
   ```bash
   rbh-sync rbh:lustre:/mnt/lustre rbh:mongo:blob --one
   ```

6. **Make a snapshot of a Mongo database to MPI File Utils file**:
   ```bash
   rbh-sync rbh:mongo:blob rbh:mpi-file:blob.mpi
   ```

7. **Synchronize a POSIX hierarchy in parallel using MPI**:
   ```bash
   mpirun -np 16 rbh-sync rbh:posix-mpi:/etc rbh:mongo:blob
   ```

SEE ALSO
--------

rbh4(5), rbh4(7)

CREDITS
-------

rbh-sync and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
------

rbh-sync and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
