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

URI
===

rbh-sync (and RobinHood4 overall) use Universal Resource Identifiers (URI) to
designate a backend from which to fetch or update metadata. These URIs are
built as follows:
    rbh:BACKEND:FSNAME[#{PATH|ID}]

Where:
    BACKEND       is the name of a backend (see below for more information)
    FSNAME        is an instance of BACKEND (e.g. path for filesystems, database
                  name for databases, ...)
    PATH/ID       is the path/id of an fsentry managed by BACKEND:FSNAME (ID
                  must be enclosed in square brackets '[ID]' to distinguish it
                  from a path)

BACKEND
=======

For each of the URI, the backend to specify must correspond to either a plugin
installed for RobinHood4 (POSIX, MongoDB, S3, ...) or a name specified in the
configuration file under the `backends` section:

    ```
    backends:
        posix-mpi:
            extends: posix
            iterator: mfu
    ```

Here, the `posix-mpi` will now be usable as a URI-compatible backend.

CREDITS
=======

rbh-sync and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
======

rbh-sync and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
