.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

############
rbh-undelete
############

This design document pertains to the rbh-undelete tool. Its goal is to provide
the possibility to recover deleted files that have been previously archived
within a database.

Functioning
===========

rbh-undelete will be a new tool based on the librobinhood library and Lustre's
API, designed to recover files that have been deleted but formerly archived
within a database. It will use Lustre's API call, llapi_hsm_import, to retrieve
the removed files. Since this call requires multiple arguments stored within the
database, this tool will utilize backend filtering mechanic to retrieve the
necessary data.

While a major part of the required arguments is already stored within the
database when performing a rbh-fsevents/rbh-sync, some are yet not handled.
Therefore, llapi_hsm_import requires the path to Lustre destination where the
file was initially stored (e.g /mnt/Lustre/my/file). There is already a path
stored inside the database. In fact, while performing a rbh-fsevents or a
rbh-sync command, a path is stored inside the database. This path is relative to
the mountpoint given in the command. For instance, if a rbh-sync is done with
the mountpoint: /mnt/lustre, this mountpoint will act as the root of the sync,
resulting in storing '/...' inside the database instead of '/mnt/lustre/...'.
Consequently, only a part of the full original path is stored, the mountpoint is
then needed to use Lustre's API call.

So, two options are available, the first is to stored the mountpoint while
performing rbh commands. The other is to put it inside the command line while
performing rbh-undelete. As we find it more user friendly, we will use the first
option, i.e, storing the mountpoint inside the database.

Nonetheless, this is not the only issue coming with the path to Lustre
destination. In fact, when an archived file is removed and rbh-fsevents is
performed again, the path is erased from the database. To address this, we can
use the same two previous solutions. We chose to add a new entry called
'old_path', that will be inserted when the archived file is deleted, in order to
permanently stored the file path inside the database.

With the mountpoint and the path available, every llapi_hsm_import's arguments
are easily retrievable with backend filtering mechanic.

Consequently, the tool will be given 3 types of information:

 * A URI_
 * A Path_ to find the correct file to restore (optional)
 * Options_ to specify the tool behaviour

Here is an example of rbh-undelete usage:

.. code:: bash

    rbh-undelete rbh:mongo:blob --list
    name: test1.c, path:/test1.c
    name: test2.c, path:/blob/test2.c

    rbh-undelete rbh:mongo:blob /test1.c --restore
    test1.c has been successfully restored

Throughout this document, we will explain what needs to be done for the
rbh-undelete of RobinHood V4 with respect to RobinHood V3's rbh-undelete, and
thus vis-à-vis of Lustre, but similar mechanisms should be applicable to any of
the backends.

URI
---

The URI will work in the exact same manner as for every other tool of the
RobinHood v4 suite (and as described in the internals document__ of
librobinhood).

__ https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst#uri

Path
----

If the user does not know the path of the file he desires to restore, he
will be able to retrieve it using --list option (e.g path: /blob)

Options
_______

The options will allow the user to switch between two rbh-undelete modes:
 * listing archived files within the given directory.
 * restoring archived files within the given directory.

Generecity
----------

Since rbh-undelete use Lustre's API call, llapi_hsm_import, to retrieve archived
files, this tool will only be available for Lustre's file systems. However, if a
proper archiving system is deployed for others reading backends, the tool could
be extended to support files from those systems. This is possible because
rbh-undelete uses the librobinhood library to interrogate the backends and
relies on the tools's generic syntax to handle readable backends.
