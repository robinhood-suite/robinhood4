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
to refer to files and objects from filesystems. When discussing Lustre
implementation, we will use the term "file", in line with RobinHood V3
terminology.

Functioning
===========

rbh-undelete will be a new tool based on the librobinhood library, designed to
recover entries that have been deleted but formerly archived. To achieve this,
the tool will create a new entry with the same metadata as the one to undelete
and then link that new entry to the archived data (stored inside a HSM backend).
This is the expected behaviour of rbh-undelete tool, consistent with RobinHood
V3's implementation.

To create the new entry, the tool will start by retrieving metadata from the
database that will allow it to recreate a version of the original entry without
the content but containing essential metadata such as the original fid (for
Lustre) and statx data (for Lustre/POSIX entries). At this point, the entry is
ready to be rebound, i.e, linking it to the content inside the HSM backend
(recreating the inode). Using rbh-undelete option, `--restore`, the user will
be able to effectively restore the entry, i.e, copying the content from the HSM
backend to the filesystem in use.

RobinHood V3 uses the file's fid for the rebinding process. By comparing the
file's fid with the ones stored inside the HSM backend, the original content
and data can be matched to the file. Once this is complete, the file is rebound
and the inode recreated. Fids belong to Lustre, as we want the tool to support
multiple backends outside Lustre, providing a path with the tool will be
necessary for the comparison process.

Consequently, the tool will be given 3 to 4 types of information:

 * A URI_
 * A fid_ to identify the file or directory (specific to Lustre)
 * A Path_ to locate the entry (for all filesystems)
 * Options_ to specify the tool behaviour

Note: rbh-undelete will then offer two ways to reference Lustre entries: using
either a fid or a path. Since fids are unique identifiers, they help distinguish
between two entries with the same name created under the same path. When a fid
is used, it replaces the path inside rbh-undelete command, the backend will then
need to interpret the input appropriately as either a fid or a path.

Throughout this document, we will explain what needs to be done for the
rbh-undelete of RobinHood V4 with respect to RobinHood V3's rbh-undelete, and
thus vis-à-vis of Lustre, but similar mechanisms should be applicable to any of
the backends.

URI
---

The URI refers to the backend where metadata are stored. It will work in the
exact same manner as for every other tool of the RobinHood v4 suite
(and as described in the internals document__ of librobinhood).

__ https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#uri

fid
---

This information will only be available for Lustre's entries (files and
directories). When a file or a directory is created in Lustre, it is assigned a
unique identifier called fid.

The user will be given the possibility to choose between using a fid or a path
to retrieve its document.

Path
----

This argument refers to the entry path including the mounpoint. It is used to
identify the original inode where the entry was created. If the user does not
know the path of the entry he wishes to restore, he will be able to retrieve it
using `--list` option (e.g path: /mnt/Lustre/blob)

Options
_______

The options will allow the user to switch between four rbh-undelete modes:
 * listing and displaying archived entries within the given directory.
 * restoring archived entries within the given directory.
 * dry running the rbh-undelete command.

Usage
-----

Here is an example of rbh-undelete usage with Lustre's elements archived:

.. code:: bash

    rbh-undelete rbh:mongo:blob --list -v 1 /mnt/lustre/
    /mnt/lustre/
    ├── test1.c  fid:0x240000400:0x6:0x0 version:1 rm_time:123456
    └── blob/  fid:0x280000400:0x2:0x0
       ├── test2.c  fid:0x200000600:0x4:0x0 version:2 rm_time:123456
       └── foo/  fid:0x100000600:0x4:0x0 rm_time:123654
           └── test3.c  fid:0x200000800:0x4:0x0 version:1 rm_time:123654

    rbh-undelete rbh:mongo:blob --restore 0x240000400:0x6:0x0
    test1.c (version 1) has been successfully restored

    rbh-undelete rbh:mongo:blob --restore 0x280000400:0x2:0x0
    test2.c (version 2) has been successfully restored
    foo/ has been successfully restored
    test3.c (version 1) has been successfully restored

Genericity
----------

Since rbh-undelete relies on librobinhood to query the backends, the tool and
syntax will be generic enough to handle all writeable backend. To do so, the
tool will add a generic undelete function. This function, will interrogate the
backend specified in the URI to retrieve the metadata needed to recreate the
version of the entry without the content. Then, after creating this version, the
function will rebind the entry, by linking it to its content (or even restoring
it depending on the user wishes).

The undelete function should not directly involve the Lustre's API or even the
statx enriching process, but instead let one backend retrieve the entry and
then call the undelete/rebind function of the backend. For instance, Lustre
will need `llapi_hsm_import` to create the entry but not POSIX. This approach
allows the function to remain generic, and could be implemented via a backend
callback for instance. Then, a generic rebind function will be called by
rbh-undelete to link the entry and its content.

Here is an example of how the callback would look like:

.. code:: c

    struct rbh_backend_operations {
    int (*undelete)(
            void *backend,
            const char *dst,
            struct stat *st,
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
a new Lustre fid, and restores the file's metadata (ownership, permissions,
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
database, but the entry's metadata is retained. Only rbh-fsevents is relevant in
this case. In fact, if rbh-sync is performed again, it would simply not insert
the entry into the database because the entry no longer exists in the
filesystem. In contrast, rbh-fsevents relies on changelogs that provides a
history of changes in the filesystem. The entry can then be determined as
deleted by the filesystem, and if it was archived, rbh-fsevents will be able to
retain its metadata information.

Thus, to address the removal of the namespace entry, we propose keeping
information related to the deleted entries, such as deletion time and file path,
within the namespace entry instead of deleting it. By doing so, the user will be
able to retrieve the path of the file using the `--list` option. Then, by
passing the path as an argument of rbh-undelete and by retrieving the metadata
from the database, all arguments from `llapi_hsm_import` call are made
available.

Here is an example of the evolution namespace entry stored inside the database:

.. code:: bash

    After performing an rbh-fsevents/rbh-sync
    namespace contains: name, path and parents

    After deletion and performing rbh-fsevents
    namespace contains: rm_time and path

Note: The rm_time field here refers to the date of the log corresponding to the
file deletion, not the actual deletion time, nor when rbh-fsevents handled the
event.

Nonetheless, keep in mind that the paths stored inside the database are not
**absolute paths**. In fact, they are **relative to the mountpoint** provided to
rbh-fsevents/rbh-sync. Because the mountpoint is not stored within the database,
we require the user to specify the full path on the command line.

Note: The user will also have the possibility to use a relative path instead.

With the full path available and every other metadata easily retrievable, the
tool will then be able to recreate the partial version of file by calling
`llapi_hsm_import`.

Finally, as Lustre can't transmit rebind commands to copytools, RobinHood will
directly calls a admin-defined command, `lhsm_rebind`, to rebind the old
archived entry with the new fid provided by Lustre. This rebind command is
mandatory because, in Lustre, archives are generally tied to inode. When an
entry is deleted and later recovered using `llapi_hsm_import`, a new fid is
assigned, which points to a new inode. As a result, the archived data is no
longer associated with any valid inode. The `lhsm_rebind` command is therefore
required to reassociate the archive with the new inode.

Versionning
-----------

**Disclaimer:** This section is specific to the HSM backend in use and may not
apply to other HSM backends.

HSM archiving allows us to store multiple versions of an entry. In fact,
whenever a entry is updated, a new version is created and the previous one is
preserved. Providing users the option to retrieve any version of a given entry
is an important feature rbh-undelete will support.

This feature will be as follows:

.. code:: bash

    rbh-undelete rbh:mongo:test --list -v 1 /blob/
    blob/
    ├── test2.c  fid:0x200000600:0x4:0x0 version:2
    └── foo/  fid:0x100000600:0x4:0x0
        └── test3.c  fid:0x200000800:0x4:0x0 version:1

    rbh-undelete rbh:mongo:test --list -v 2 0x100000600:0x4:0x0
    version:1 update_date:123456
    version:2 update_date:123456

    rbh-undelete rbh:mongo:test --restore 0x100000600:0x4:0x0 --version 1
    test3.c (version1) has been successfully restored

Note: Unless specified, the latest version of the entry will be restored
