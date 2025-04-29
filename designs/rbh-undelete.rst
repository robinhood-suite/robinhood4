.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

############
rbh-undelete
############

This design document pertains to the rbh-undelete tool. Its goal is to provide
the possibility to recover deleted entries that have been previously archived.
This can be done using their metadata, that are still stored in RobinHood
database.

To simplify the syntax, the term "entry" will be used throughout this document
to refer to files and objects. When discussing Lustre implementation, we will
use the term "file", in line with RobinHood V3 terminology.

Functioning
===========

rbh-undelete will be a new tool based on the librobinhood library, designed to
recover entries that have been deleted but formerly archived. To achieve this,
the tool will create a new entry with the same metadata as the one to undelete
and then link that new entry to the archived data (stored inside an HSM
backend).

To create the new entry, the tool will first retrieve the metadata of the entry
from the database, and then create it anew with same metadata. At this point,
the entry is ready to be rebound, i.e, its archived version in the HSM backend
can be linked to the newly-created entry.

The rebinding process does not require restoring the content from the HSM
backend to the storage system in use, it only links the content to the new
entry. This can be done by using the `--restore` option.

Consequently, the tool will be given 3 types of information:

 * A URI_ to identify the backend where the file's metadata are stored
 * Another URI_ to locate the entry to undelete
 * Options_ to specify the tool behaviour

Throughout this document, we will explain what needs to be done for the
rbh-undelete of RobinHood V4 with respect to RobinHood V3's rbh-undelete, and
thus vis-à-vis of Lustre, but similar mechanisms should be applicable to any of
the backends.

URI
---

Rbh-undelete uses URIs to identify backends. It will work in the exact same
manner as for every other tool of the RobinHood v4 suite (and as described in
the internals document__ of librobinhood).

__ https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#uri

Options
_______

The options will allow the user to switch between three rbh-undelete modes:
 * listing and displaying archived entries and versions.
 * restoring an archived entry with a path or the entries in a given directory
 * dry running the rbh-undelete command.

Usage
-----

Here is an example of rbh-undelete usage with Lustre's elements archived:

.. code:: bash

    rbh-undelete rbh:mongo:blob --list /mnt/lustre/
    /mnt/lustre/
    ├── test1.c  rm_time:Wed Jul 16 10:25:53 2025
    └── blob/
        ├── test2.c  rm_time: Wed Jul 16 10:53:10 2025
        └── foo/  rm_time:Wed Jul 16 11:28:02 2025
            └── test3.c  Wed Jul 16 11:28:02 2025

    rbh-undelete rbh:mongo:blob --restore rbh:lustre:/mnt/lustre/blob/foo/test3.c
    test3.c has been rebound to its content.

Genericity
----------

Since rbh-undelete relies on librobinhood to query the backends, the tool and
syntax will be generic enough to handle all writeable backends. To do so, the
tool will add a generic undelete function that interrogates the backend
specified in the first URI to retrieve the metadata needed to recreate the
version of the entry without the content. This entry is recreated in the
backend specified in the second URI.

Each backend must define its own rebinding/restoration functions. The tool only
interprets the backends and the options specified within the command line. Then,
rbh-undelete retrieves the necessary metadata and transmits it to the
appropriate backends.

This approach allows the function to remain generic, and could be implemented
via a backend callback for instance.

Here is an example of how the callback would look like:

.. code:: c

    struct rbh_backend_operations {
    int (*undelete)(
            void *backend,
            const char *dst,
            struct rbh_fsentry *fsentry,
            );
            .
            .
    };

Lustre's implementation
-----------------------

The implementation will follow the same approach as RobinHood V3 for handling
Lustre entries.

RobinHood V3 based its undelete implementation on a Lustre's API call,
`llapi_hsm_import`. This call creates a file in a *released* state, assigns it
a new Lustre FID, and restores the file's metadata (ownership, permissions,
timestamps, etc.) from the database. To use this call, metadata stored inside
the database are required.

Here is the `llapi_hsm_import` call with its arguments:

.. code:: c

    /**
    * Import an existing hsm-archived file into Lustre.
    *
    * Caller must access file by (returned) newfid value from now on.
    *
    * \param dst       path to Lustre destination (e.g. /mnt/lustre/my/file).
    * \param archive   archive number.
    * \param st        struct stat buffer containing file ownership, perm, etc.
    * \param stripe_*  Striping options.  Currently ignored, since the restore
    *                  operation will set the striping. In V2, this striping
    *                  might be used.
    * \param pool_name Name of the OST pool to use for file stripping.
    * \param newfid[out] Filled with new Lustre fid.
    */
    int llapi_hsm_import(const char *dst, int archive, const struct stat *st,
                         unsigned long long stripe_size, int stripe_offset,
                         int stripe_count, int stripe_pattern, char *pool_name,
                         struct lu_fid *new_fid);

Here, the archive number, stat structure, striping options and pool_name are
all stored in the database. On the other hand, the destination path is not
fully available.

Initially, paths are stored under the namespace entry within the database as
part of the hardlink information. However, when an archived file is removed and
rbh-fsevents is performed again, the namespace entry is removed from the
database, but the entry's metadata is retained.

In fact, if rbh-sync is performed again, it would simply not insert the entry
into the database because the entry no longer exists in the filesystem. In
contrast, rbh-fsevents relies on changelogs that provides a history of changes
in the filesystem. The entry can then be determined as deleted by the
filesystem, and if it was archived, rbh-fsevents will be able to retain its
metadata information.

Thus, to address the removal of the namespace entry, we propose keeping
information related to the deleted entries, such as deletion time and file path,
within the namespace entry instead of deleting it. By doing so, the user will be
able to retrieve the path of the file using the `--list` option. Then, by
passing the path as an argument of rbh-undelete and by retrieving the metadata
from the database, all arguments from `llapi_hsm_import` call are made
available.

Here is an example of the evolution namespace entry stored inside the database:

.. code:: text

    After performing an rbh-fsevents/rbh-sync
    namespace contains: path and parent

    After deletion and performing rbh-fsevents
    namespace contains: rm_time and path

Nonetheless, paths stored inside the database are not **absolute paths**. In
fact, they are **relative to the mountpoint** provided to rbh-fsevents/rbh-sync.
Because the mountpoint is not stored within the database, we require the user to
specify the full path on the command line while undeleting Lustre files.

Note: The user will also have the possibility to use a relative path instead. In
this case, the path must be relative to the current working directory of
rbh-undelete.

With the full path available and every other metadata easily retrievable, the
tool will then be able to recreate the partial version of file by calling
`llapi_hsm_import`.

Finally, when some extended attributes are missing from the database, RobinHood
will directly call an admin-defined command, `lhsm_rebind`. The latter asks
the copytool to rebind the entry with the first FID (FID from the original
removed file) to the name of of the second FID (essentially rename the file in
the HSM backend) and set the extended attributes accordingly.

Versioning
----------

**Disclaimer:** This section is specific to the HSM backend in use and may not
apply to other HSM backends.

Some HSM backends, such as Phobos (`phobos <https://github.com/cea-hpc/phobos>`_
& `lustre-hsm-phobos <https://github.com/phobos-storage/lustre-hsm-phobos>`_)
support versioning, meaning multiple versions of the same entry are available
for restoration. Thus, via an external command, we will allow displaying all
versions of an archived entry, and restoring a particular version of that entry
when undeleting it.

This feature will be as follows:

.. code:: bash

    rbh-undelete rbh:mongo:test --list -v /blob/
    blob/
    ├── test2.c  fid:0x200000600:0x4:0x0 version:2
    └── foo/  fid:0x100000600:0x4:0x0
        └── test3.c  fid:0x200000800:0x4:0x0 version:1

    rbh-undelete rbh:mongo:test --list -v 0x100000600:0x4:0x0
    version:1 update_date:123456
    version:2 update_date:123456

    rbh-undelete rbh:mongo:test --restore 0x100000600:0x4:0x0 --version 1
    test3.c (version1) has been successfully restored

Note: Unless specified, the latest version of the entry will be restored
