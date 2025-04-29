.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

############
rbh-undelete
############

This design document pertains to the rbh-undelete tool. Its goal is to provide
the possibility to recover deleted files or objects that have been previously
archived using a database and HSM backend.

To simplify the syntax, the term "file" will be used throughout this document,
although the same principles apply to objects.

Functioning
===========

rbh-undelete will be a new tool based on the librobinhood library, designed to
recover files that have been deleted but formerly archived. Since the data and
metadata of archived files are stored in two different backend (HSM backend and
a database, respectively), this tool will utilize backend filtering mechanic to
retrieve the necessary informations.

To begin with, we need to retrieve metadata that will allow the tool to recreate
a *partial* version of the original file, i.e, a version without the actual
file content but containing essential metadata such as the original fid (for
Lustre) and statx data (for Lustre/POSIX files). At this point, the file is
ready to be rebound, i.e, retrieving its content from the HSM backend.

RobinHood V3 uses the file's fid for this process. By comparing the file's fid
with the ones stored inside the HSM backend, the original content and
data can be matched to the file. Once this is complete, the file is rebound and
effectively undeleted.

This is the expected behaviour of rbh-undelete tool, consistent with RobinHood
V3 implementation.

Implementation
--------------

The implementation in this design, and therefore in RobinHood V4, will follow
the same approach as RobinHood V3 for handling Lustre files.

RobinHood V3 based its undelete implementation on a Lustre's API call,
`llapi_hsm_import`. This call creates a file in a *released* state, assigns it a
new Lustre fid, and restores the file's metadata (ownership, permissions,
timestamps, etc.) from the archive. To use this call, metadata stored inside
the database are required.

Here is the `llapi_hsm_import` call with its arguments:

.. code:: c

    /**
    * Import an existing hsm-archived file into Lustre.
    *
    * Caller must access file by (returned) newfid value from now on.
    *
    * \param dst      path to Lustre destination (e.g. /mnt/lustre/my/file).
    * \param archive  archive number.
    * \param st       struct stat buffer containing file ownership, perm, etc.
    * \param stripe_* Striping options.  Currently ignored, since the restore
    *                 operation will set the striping. In V2, this striping
                      might be used.
    * \param newfid[out] Filled with new Lustre fid.
    */
    int llapi_hsm_import(const char *dst, int archive, const struct stat *st,
                         unsigned long long stripe_size, int stripe_offset,
                         int stripe_count, int stripe_pattern, char *pool_name,
                         struct lu_fid *new_fid);

Although every metadata - except the path destination - required for this call
are stored inside the database after running rbh-fsevents or rbh-sync, `*dst` is
not handle properly by RobinHood V4.

Initially paths are stored under the namespace entry alongside hardlink
information. However, when an archived file is removed and rbh-fsevents is
performed again, the namespace entry is removed from the database. To address
this, we propose storing information related to the deleted files, such as
deletion time and file path, within the namespace entry instead of deleting it.
Combined with backend filtering, rbh-undelete will be capabable of retrieving
the path of deleted file using rbh-undelete --list option.

Here is an example of namespace entry schema stored inside the database:

.. code:: bash

    "ns": { "xattrs": { "rm": { "time": 123456, "path": "/blob" } } }

Note: The time field here refers to when rbh-fsevents detected the file
deletion, not the actual deletion time.

Nonetheless, keep in mind that the paths stored inside the database are not
**absolute paths**. In fact, they are **relative to the mountpoint** provided to
rbh-fsevents/rbh-sync. Because the mountpoint is not stored within the database,
we require the user to specify the full path on the command line following V3
implementation.

Note: If executed in the right directory, the user will have the possibility to
use a relative path instead.

With the full path available and every other metadata easily retrievable, the
tool will then be able to recreate the partial version of file by calling
`llapi_hsm_import`.

As Lustre can't transmit rebind commands to copytools, RobinHood will directly
calls a admin-defined command, `lhsm_rebind`, to rebind the old archived entry
with the new fid provided by Lustre.

Consequently, the tool will be given 3 to 4 types of information:

 * A URI_
 * A fid_ to identify the file or directory (specific to Lustre)
 * A Path_ to locate the file
 * Options_ to specify the tool behaviour

Note: rbh-undelete offers two ways to reference Lustre files: using either a fid
or a path. Since fids are unique identifiers, they help distinguish between
two files with the same name created under the same path. When a fid is used,
it replaces the path inside rbh-undelete command, the backend will then need to
interpret the input appropriately as either a fid or a path.

Here is an example of rbh-undelete usage with Lustre's elements archived:

.. code:: bash

    rbh-undelete rbh:mongo:blob --list /mnt/lustre/
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

This information will only be available for Lustre's files and directories.
When a file or a directory is created in Lustre, it is assigned a unique
identifier called fid.

Path
----

This argument refers to the file path excluding the mounpoint. It is used to
identify the original inode where the file was created. If the user does not
know the path of the file he wishes to restore, he will be able to retrieve it
using --list option (e.g path: /blob)

Options
_______

The options will allow the user to switch between four rbh-undelete modes:
 * listing archived files within the given directory.
 * displaying archived files versions.
 * restoring archived files within the given directory.
 * dry running the rbh-undelete command.

Versionning
-----------

HSM archiving allows us to store multiple versions of a file. In fact, whenever
a file is updated, a new version is created and the previous one is preserved.
Providing users the option to retrieve any version of a given file is an
important feature rbh-undelete will support.

This feature will be as follows:

.. code:: bash

    rbh-undelete rbh:mongo:test --list /blob/
    blob/
    ├── test2.c  fid:0x200000600:0x4:0x0 version:2
    └── foo/  fid:0x100000600:0x4:0x0
        └── test3.c  fid:0x200000800:0x4:0x0 version:1

    rbh-undelete rbh:mongo:test --list --version 0x100000600:0x4:0x0
    version:1 update_date:123456
    version:2 update_date:123456

    rbh-undelete rbh:mongo:test --restore 0x100000600:0x4:0x0 --version 1
    test3.c (version1) has been successfully restored

Note: Unless specified, the latest version of the file will be restored

Generecity
----------

Since rbh-undelete relies on librobinhood to query the backends, the tool and
syntax will be generic enough to handle all writeable backend. To do so, the
tool will add a generic undelete function and another function, `lhsm_rebind`,
to rebind the file to its content.

The undelete function should not directly involved the Lustre's API or even the
statx enriching process, but instead let the backend return the *partial*
version of the file. For instance, Lustre will need `llapi_hsm_import` to create
this file but not POSIX. This approach allows the function to remain generic,
and could be implemented via a backend callback for instance that returns this
version of the file. Then, a generic rebind function will be called by
rbh-undelete to link the file and its content.

Here is an example of how callback would look:

.. code:: c

    struct rbh_backend_operations {
    int (*undelete)(
            void *backend,
            const char *dst,
            struct rbh_undelete_statx *statx,
            struct rbh_undelete_lustre_stat lstat
            );
            .
            .
    };

With the structs `rbh_undelete_statx` and `rbh_undelete_lustre_stat` containing
essentials statx and lustre relative stats, respectively.
