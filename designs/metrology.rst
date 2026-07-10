.. This file is part of RobinHood
   Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

#########
Metrology
#########

This design document pertains to RobinHood 4's metrology. Its goal is to provide
statistics about the commands, with regard to their time taken and entries seen,
for both benchmark and operation control purposes.

The metrology's statistics will be given during the tools' execution. They will
show information about the number of entries seen per second, number of entries
managed, the number of errors, ... Overall, the same type of information
displayed during a RobinHood 3 command will similarly be shown during a
RobinHood 4 command. Moreover, while the statistics will be shown during the
command's execution, they will also be recorded in the mirror backend if
possible, and in a log file if requested.

Gathering statistics
====================

Regarding the type of statistics to gather, we can distinguish two categories:
command-specific stats and plugin-specific stats.

Command statistics
------------------

The first kind of statistics are about the command themselves and their usage.
For instance, when did an invocation of `rbh-fsevents` start or how long did a
run of `rbh-sync` last for. The commands targeted will only be those which may
take a long time, i.e. not `rbh-info`-like commands. The statistics will be
updated by those commands during their runtime.

To gather the statistics, the commands will use information only they know
about, like the start timestamp or the command line, and information given by
the plugins, but general to the command's usage (not to be confused with the
plugin-specific statistics, it is only a matter of source here). For instance,
the number of entries converted from the original system to `rbh_fsentry` or
the number of skipped entries.

Both of the source for statistics (command and plugin) are already partly
implemented, mainly in `rbh-sync`. As an example, it already records the start
time of its invokation, the duration of a synchronization, the mountpoint, ...
Moreover, it uses the `rbh_metadata` structure given to the
`rbh_backend_filter` function to gather the aforementionned command-related
statistics by the plugin.

We thus plan to extend this structure and its usage. Here is the list of
important command statistics we propose of recording. Some may be added or
removed, depending on if they are needed or not:

+------------------------------+-----------------------------+--------------------------+
| Command \ statistic's source | Command                     | Plugin                   |
+==============================+=============================+==========================+
| all commands                 | start time                  |                          |
|                              | end time                    |                          |
|                              | duration                    |                          |
|                              | command line                |                          |
+------------------------------+-----------------------------+--------------------------+
| rbh-sync                     | source mountpoint           | converted entries        |
|                              | last sync time              | skipped entries          |
                               |                             | total entries            |
+------------------------------+-----------------------------+--------------------------+
| rbh-fsevents                 | source read                 | amount of changelog read |
|                              | enrich mountpoint           | current changelog index  |
|                              | worker count                | enrich skip count        |
|                              | deduplication ratio         |                          |
+------------------------------+-----------------------------+--------------------------+
| rbh-find                     | entry-count after filtering |                          |
                               | -exec entry count success   |                          |
+------------------------------+-----------------------------+--------------------------+
| rbh-report                   |                             |                          |
+------------------------------+-----------------------------+--------------------------+
| rbh-gc                       | sync-time used              |                          |
|                              | deleted entry count         |                          |
                               | non-deleted entry count     |                          |
|                              | total entries               |                          |
+------------------------------+-----------------------------+--------------------------+

With this list, we will need to add statistics gathering to all the commands
that require it, and add additional fields to the `rbh_metadata` structure:

.. code:: c

    struct rbh_metadata {
        struct rbh_common_metadata common_md;
        union {
            struct rbh_sync_metadata sync_md;
            struct rbh_fsevents_metadata fsevents_md;
            struct rbh_find_metadata find_md;
            struct rbh_report_metadata report_md;
            struct rbh_gc_metadata gc_md;
        }
    };

Each sub-structure will have fields corresponding to the statistics given in the
table above, and may be removed if unnecessary.

Plugin statistics
-----------------

The plugin-specific statistics are information only plugins know about. For
instance, how many directories were filtered by the POSIX plugin, or how many
MPI processes were used with an MPI iterator. They will be updated by the
commands using those plugins during their runtime, relying on the plugins
themselves to provide said statistics.

API
+++

As basically every plugin can provide additional information about what it
wants, having one defined structure given to the plugins is not feasible.
Therefore, we will instead simply use a `void *` pointer as part of the
`rbh_metadata` structure that the plugins will allocate, use and free, to
record statistics deemed of interest.

.. code:: c

    struct rbh_metadata {
        void *plugin_md;
        struct rbh_common_metadata common_md;
        union {
            struct rbh_sync_metadata sync_md;
            struct rbh_fsevents_metadata fsevents_md;
            struct rbh_find_metadata find_md;
            struct rbh_report_metadata report_md;
            struct rbh_gc_metadata gc_md;
        }
    };

Regarding the C API, there will be modifications to the two main functions for
iterating through and updating entries, respectively the `rbh_backend_filter`
and `rbh_backend_update` functions. Both of these calls will make use of the
`rbh_metadata` structure to record various information, which the commands
themselves may then use to update in real time the user, and add to the mirror
backend.

The `rbh_metadata` structure is already part of the `rbh_backend_filter` call,
so this structure only needs to be added to the `rbh_backend_update` call.
Here is how the API will be modified for these changes:

.. code:: c

    struct rbh_mut_iterator * rbh_backend_filter(
        struct rbh_backend *backend,
        const struct rbh_filter *filter,
        const struct rbh_filter_options *options,
        const struct rbh_filter_output *output,
        struct rbh_metadata *metadata
    );

    ssize_t rbh_backend_update(
        struct rbh_backend *backend,
        struct rbh_iterator *fsevents,
        struct rbh_metadata *metadata
    );

Printing and recording statistics
=================================

After gathering those statistics, the different commands will use them to give
info to the user or record them in a separate log file, and update the mirror
backend as logs.

Printing statistics
-------------------

Regarding the exact statistics printed, they will of course include the ones
gathered by the command from their usage and the plugins, but also the result
of calculations done on them. For instance (the main goal of this metrology), we
will print the number of entries managed by second for each command, the number
of entries updated per second, per process, the space taken (if available), ...

While the non-plugin specific statistics are easy enough to manipulate, the same
cannot be said about the plugin specific ones. Since those are recorded as
an abstract structure, we will need a new function to convert this structure to
a `rbh_value_map`:

.. code:: c

    struct rbh_pe_common_operations {
        /**
         * Convert a \p metadata structure to a string for printing.
         *
         * @param metadata           the metadata structure to convert
         * @param metadata_map       the output metadata as a value map, content
         *                           should be freed by the caller
         */
        void (*metadata2map)(struct rbh_metadata *metadata,
                             struct rbh_value_map *metadata_map);
    };

    static inline int
    rbh_pe_common_ops_metadata2map(
        const struct rbh_pe_common_operations *common_ops,
        struct rbh_metadata *metadata,
        struct rbh_value_map *metadata_map
    )
    {
        if (common_ops && common_ops->metadata2map)
            return common_ops->metadata2map(metadata, metadata_map);

        errno = ENOTSUP;
        return 1;
    }

Finally, using this function and other conversion functions, we can print out
the statistics to the standard output, following RobinHood3's formating, which
will look like this (adapted for each command of course)::

    STATS | ==================== Dumping stats at 2025/02/17 00:01:44 =====================
    STATS | ======== General statistics =========
    STATS | Daemon start time: 2025/02/11 15:57:23
    STATS | Started modules: scan
    STATS | ======== FS scan statistics =========
    STATS | current scan interval = 7.0d
    STATS | scan is running:
    STATS |      started at : 2025/02/11 15:57:23 (5.3d ago)
    STATS |      last action: 2025/02/17 00:01:43 (01s ago)
    STATS |      progress   : 418108230 entries scanned (2 errors)
    STATS |      inst. speed (potential):    469.12 entries/sec (51.16 ms/entry/thread)
    STATS |      avg. speed  (effective):    906.84 entries/sec (26.24 ms/entry/thread)
    STATS | ==== EntryProcessor Pipeline Stats ===
    STATS | Idle threads: 23
    STATS | Id constraints count: 1000 (hash min=0/max=3/avg=0.1)
    STATS | Name constraints count: 1000 (hash min=0/max=3/avg=0.1)
    STATS | Stage              | Wait | Curr | Done |     Total | ms/op |
    STATS |  0: GET_FID        |    0 |    0 |    0 |       320 |  0.77 |
    STATS |  1: GET_INFO_DB    |    0 |    0 |    0 |     31720 |  0.25 |
    STATS |  2: GET_INFO_FS    |    0 |    0 |    0 |     31720 |  0.04 |
    STATS |  3: PRE_APPLY      |    0 |    0 |    0 |     31720 |  0.00 |
    STATS |  4: DB_APPLY       |  995 |    5 |    0 |     31720 |  1.86 | 80.88% batched (avg batch size: 4.1)
    STATS |  5: CHGLOG_CLR     |    0 |    0 |    0 |         0 |  0.00 |
    STATS |  6: RM_OLD_ENTRIES |    0 |    0 |    0 |         0 |  0.00 |
    STATS | DB ops: get=547/ins=397134437/upd=20981752/rm=0

Printing to a log file
----------------------

As most RobinHood4 mirror-updating commands are run through automated tools
(cron, the policy engine, ...), we need to be able to print the statistics as
logs to a separate log file, dissociated from regular usage logs. To do this,
we must simply add a `log-file` option to `rbh-sync`, `rbh-fsevents` and
`rbh-gc`:

.. code:: bash

    -L, --log-file FILE     the log file statistics should be printed to

Recording in the mirror backend
-------------------------------

The recording of statistics as logs is already implemented with the
`rbh_backend_insert_metadata` call, meaning it is only a matter of formatting
the statistics to a `rbh_value_map` structure on the command side, and the
recording is done at the end of the commands' runtime. Therefore, only the
equivalent of the last log printed will be recorded, as it contain information
gathered during the whole command runtime.

Viewing statistics as logs
==========================

To manage older logs, we will implement a new command, `rbh-log`. It will rely
on the same mechanisms as `rbh-info`, but since that command already has
multiple jobs, adding more to it would be confusing. So we instead will shift
part of its job to the log management specific command.

`rbh-info` already has implemented options to get information about the first
and the last `rbh-sync` runs. Therefore, we will have to extend this log
viewing mechanism to allow getting information about all previous commands and
their runs.

Currently, logs are recorded using an incrementing ID in the mirror backend. So
we can use this ID to get information about a specific log, which will be given
by the caller. Moreover, we will add options to see the last logs of a certain
command, see the last N logs, and another to view the logs in a shortened
format, à la `git log --oneline`. Here is what `rbh-log` will look like:

.. code:: bash

    $ rbh-log --help
    Usage: rbh-log [LOG_OPTIONS] BACKEND

    Positional arguments:
        BACKEND          a URI describing a RobinHood4 backend

    Log optional arguments:
        --stat [-]N      print the Nth command call log, starting from 0 (`N`)
                         or from the last log (`-N`)
        --last N         print the last N log
        --sync N         print the last N log of rbh-sync runs
        --fsevents N     print the last N log of rbh-fsevents runs
        --find N         print the last N log of rbh-find runs
        --report N       print the last N log of rbh-report runs
        --gc N           print the last N log of rbh-gc runs
        --oneline        print logs in a shortened format

    $ rbh-log <URI> --oneline --sync 3
    { start: Fri Jun 26 14:31:39 2026, duration: 1s, entries converted: 4183, total entries: 4183 }
    { start: Fri Jun 26 14:31:48 2026, duration: 3s, entries converted: 27, total entries: 4720 }
    { start: Fri Jun 26 14:31:59 2026, duration: 5s, entries converted: 1, total entries: 400 }


Removing logs
=============

As logs are added for each command, and so indefinitely, users need a way to
remove older logs without having to tinker with the inner workings of the
mirror backends. To do this, we will add a `--delete` to the options of
`rbh-log`, transforming the options detailed above to delete the logs instead
of printing them:

.. code:: bash
    $ rbh-log --help
    Usage: rbh-log [--delete] [STAT_OPTIONS] BACKEND

    Positional arguments:
        BACKEND          a URI describing a RobinHood4 backend

    Optional arguments:
        --delete         delete the requested logs instead of printing

    Log optional arguments:
        --stat [-]N      print the Nth command call log, starting from 0 (`N`)
                         or from the last log (`-N`)
        --last N         print the last N log
        --sync N         print the last N log of rbh-sync runs
        --fsevents N     print the last N log of rbh-fsevents runs
        --find N         print the last N log of rbh-find runs
        --report N       print the last N log of rbh-report runs
        --gc N           print the last N log of rbh-gc runs
        --oneline        print logs in a shortened format

External monitoring tools
=========================

At the time of writing this document, we have no need to interface RobinHood4
with any monitoring tool, so this will not be implemented. The expected
behaviour will, until further notice, to run RobinHood4 commands and use their
output for minotoring.
