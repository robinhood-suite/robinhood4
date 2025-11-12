rbh-sync
========

NAME
====

rbh-sync - synchronize metadata from one backend to another

SYNOPSIS
========

rbh-sync [PRE-OPTIONS]... SOURCE DEST [POST-OPTIONS]...

DESCRIPTION
===========

Synchronize metadata from SOURCE to DEST. rbh-sync will iterate through every
entry in SOURCE, and for each one, will copy its metadata to DEST. The
metadata retrieved depends on the type of SOURCE being used. Both SOURCE and
DEST must take the form of URIs.

Mandatory arguments to long options are mandatory for short options too.

PRE-OPTIONS
===========

--alias=ALIAS     specify an alias for the operation. Aliases are a way to
                  shorten the command line by specifying frequently used
                  components in the configuration file under an alias name, and
                  using that alias name in the command line instead.
-c, --config=PATH
                  the configuration file to use, if not specified, will use
                  `/etc/robinhood4.d/default.yaml`

POST-OPTIONS
============

-d, --dry-run     displays the command after alias replacing every alias in it,
                  and stop the command
-f, --field=[+-]FIELD
                  select, add or remove a FIELD to synchronize (can be specified
                  multiple times)

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

-n, --no-skip     do not skip errors when synchronizing metadata. By default,
                  if an entry-related error occurs during rbh-sync's run, it is
                  skipped.
-o, --one         only consider the root of SOURCE and do not synchronize
                  anything else

EXAMPLES
========

Synchronize a POSIX hierarchy to a Mongo database:
    $ rbh-sync rbh:posix:/etc rbh:mongo:blob

Synchronize only a Lustre mountpoint to a SQLite database:
    $ rbh-sync rbh:lustre:/mnt/lustre rbh:sqlite:blob.sqlite --one

Make a snapshot of a Mongo database to MPI File Utils file:
    $ rbh-sync rbh:mongo:blob rbh:mpi-file:blob.mpi

SEE ALSO
========

rbh4(5), rbh4(7)

CREDITS
=======

rbh-sync and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
======

rbh-sync and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
