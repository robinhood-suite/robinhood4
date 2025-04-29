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
and then link that new entry to the archived data (stored inside a HSM backend).

To create the new entry, the tool will first retrieve the metadata of the entry
from the database, and then create it anew with some metadata. At this point,
the entry is ready to be rebound, i.e, its archived version in the HSM backend
can be linked to the newly-created entry.
To summarize, a new entry is created based on the metadata from the old entry.
This new entry is then linked to the archive (which contains the content of the
old entry) through the rebinding process.

Rebinding refers to linking a newly created inode to the archiving system.
Before this step, the data from the original entry (stored inside the HSM
backend) is no longer accessible, as its original inode has been removed. By
creating a new inode - with the same metadata as the original - and linking it
to the data in the HSM backend, access to the file content can be restored threw
the new inode.
This process does not initially include copying the content from the HSM backend
to the filesystem in use, it only links the content to the new entry. Using
rbh-undelete option, `--restore`, the user will be able to effectively copying
the entry to their filesystem.

By comparing an entry identifier to the ones stored inside the HSM backend, the
original content and data can be matched to the file. Once this is complete,
the inode is recreated and the file is rebound.

Consequently, the tool will be given 3 types of information:

 * A URI_
 * An Arguments_list_ to identify the entry (backend-related identifiers)
 * Options_ to specify the tool behaviour

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

Arguments-list
--------------

This list of arguments is used to identify the original entry. Depending on the
backend, the argument(s) provided may vary. Since the identifier is
backend-related, the backend must interpret the input appropriately.

For instance, Lustre backend accepts both a FID and a Path, whereas POSIX only
requires a Path to the destination.

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

    rbh-undelete rbh:mongo:blob --list /mnt/lustre/
    /mnt/lustre/
    ├── test1.c  FID:0x240000400:0x6:0x0 version:1 rm_time:123456
    └── blob/  FID:0x280000400:0x2:0x0
        ├── test2.c  FID:0x200000600:0x4:0x0 version:2 rm_time:123456
        └── foo/  FID:0x100000600:0x4:0x0 rm_time:123654
            └── test3.c  FID:0x200000800:0x4:0x0 version:1 rm_time:123654

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
backend will rebind the entry, by linking it to its content (or even restoring
it depending on the user wishes).

The undelete function should not directly involve any backend-related processes
such as the usage of Lustre's API or even the statx enriching process, but
instead let one backend retrieve the entry and then call the undelete/rebind
function of the backend. For instance, Lustre will need `llapi_hsm_import` to
create the entry but not POSIX.

This approach allows the function to remain generic, and could be implemented
via a backend callback for instance.

Here is an example of how the callback would look like:

.. code:: c

    struct rbh_backend_operations {
    int (*undelete)(
            void *backend,
            const char *dst,
            struct rbh_statx *st,
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

.. code:: bash

    After performing an rbh-fsevents/rbh-sync
    namespace contains: name, path and parent

    After deletion and performing rbh-fsevents
    namespace contains: rm_time and path

Note: The rm_time field here refers to the date of the log corresponding to the
file deletion, not the actual deletion time, nor when rbh-fsevents handled the
event.

Nonetheless, keep in mind that the paths stored inside the database are not
**absolute paths**. In fact, they are **relative to the mountpoint** provided to
rbh-fsevents/rbh-sync. Because the mountpoint is not stored within the database,
we require the user to specify the full path on the command line while
undeleting Lustre files.

Note: The user will also have the possibility to use a relative path instead.

With the full path available and every other metadata easily retrievable, the
tool will then be able to recreate the partial version of file by calling
`llapi_hsm_import`.

Finally, as Lustre can't transmit rebind commands to copytools, RobinHood will
directly call an admin-defined command, `lhsm_rebind`, to rebind the old
archived entry with the new FID provided by Lustre. This rebind command is
mandatory because, in Lustre, archives are generally tied to an inode. When an
entry is deleted and later recovered using `llapi_hsm_import`, a new FID is
assigned, which points to a new inode. As a result, the archived data is no
longer associated with any valid inode. The `lhsm_rebind` command is therefore
required to reassociate the archive with the new inode.

Versioning
----------

**Disclaimer:** This section is specific to the HSM backend in use and may not
apply to other HSM backends.

Some HSM backends, such as Phobos (`phobos <https://github.com/cea-hpc/phobos>`_
& `lustre-hsm-phobos <https://github.com/phobos-storage/lustre-hsm-phobos>`_)
support versioning, meaning multiple versions of the same entry are available
for restoration. Thus, we will allow displaying all versions of an archived
entry, and restoring a particular version of that entry when undeleting it.

This feature will be as follows:

.. code:: bash

    rbh-undelete rbh:mongo:test --list /blob/
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
