.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#############
Run retention
#############

This document describes how to run and monitor the retention feature of
RobinHood 4.

Day-to-day running
==================

Once installed, the RobinHood 4 suite must be run regularly to be effective.
Since the goal is to target specific hierarchy (for instance users) and over
a long period of time, running the suite daily is enough to ensure proper
detection of expired files or directories.

When doing so, 3 tools are especially important, `rbh-sync` to synchronize a
POSIX hierarchy to the Mongo database, `rbh-lfind` to query expired files,
and `rbh-update-retention` to apply the special retention logic to directories.

Scripts
=======

To use these tools in a user-friendly and automated way, we provide here scripts
that can be deployed on your system.

Utils script
------------

This is a utilitary script used by the two other for common variables and
information.

.. code:: sh

    #!/bin/sh

    # Configuration file location
    conf_file="/etc/robinhood4.d/rbh_conf.yaml"

    # Path to users homes
    SYNC_ROOT_TEMPLATE="/somewhere"

    # Users home to sync, each value in this array, if combined with
    # 'SYNC_ROOT_TEMPLATE' should be a valid path to a user's directory
    SYNC_ROOT_STORES=("users")

    # Template for the database name
    db_name_template="retention"

    # Get a user specific database name
    function get_database_name()
    {
        local user="$1"

        echo "${db_name_template}-${user}"
    }

    # Get the full path to synchronize, $1 should be a value in
    # 'SYNC_ROOT_STORES'
    function get_full_sync_path()
    {
        local home="$1"

        echo "${SYNC_ROOT_TEMPLATE}/$home"
    }


Sync script
-----------

This is a script wrapping `rbh-sync` calls to synchronize a user's hierarchy to
a Mongo database dedicated to them.

.. code:: sh

    #!/bin/sh

    source retention_utils_script.bash

    function timed_sync()
    {
        local user="$1"
        local home="$2"
        local user_db="$3"

        local sync_root="$(get_full_sync_path "$home")"
        local log_file="retention-sync-${user}-$(date +%s).log"

        touch $log_file

        begin=$(date +%s)
        rbh-sync --config $conf_file "rbh:retention:$sync_root" rbh:mongo:$user_db >> $log_file
        end=$(date +%s)

        time=$(echo "$end - $begin" | bc)
        echo "Sync of '$sync_root' took '$time' seconds" >> $log_file
    }

    for home in "${SYNC_ROOT_STORES[@]}"; do
        user="$(echo "$home" | cut -d'/' -f2)"
        user_db="$(get_database_name "$user")"

        mongo --quiet "$user_db" --eval "db.dropDatabase()"
        timed_sync "$user" "$home" "$user_db"
    done

Update-retention script
-----------------------

This is a script wrapping the `rbh-update-retention` call to find expired
directories, and remove them if needed.

.. code:: sh

    #!/bin/sh

    source retention_utils_script.bash

    DELETE=${DELETE:-false}

    function timed_retention()
    {
        local user="$1"
        local home="$2"
        local user_db="$3"

        local sync_root="$(get_full_sync_path "$home")"
        local log_file="retention-update-${user}-$(date +%s).log"

        touch $log_file

        # Update directory expiration date and delete expired directories
        begin=$(date +%s)

        if [ "${DELETE}" == "true" ]; then
            output="$(rbh-update-retention rbh:mongo:${user_db} $sync_root --config $conf_file --delete 2>&1)"
            end=$(date +%s)
            echo "rbh-update-retention found and deleted expired entries in '$sync_root'" >> $log_file
        else
            output="$(rbh-update-retention rbh:mongo:${user_db} $sync_root --config $conf_file 2>&1)"
            end=$(date +%s)
            echo "rbh-update-retention found expired entries in '$sync_root'" >> $log_file
        fi

        echo "$output" | sed -e "s#/#'$sync_root/#" >> $log_file

        local time="$(echo "$end - $begin" | bc)"
        echo "Finding and updating expired entries in '$user_db' took '$time' seconds" >> $log_file

        # Uncomment this line to add automated mail sending
        # cat $log_file | mail -s "[Retention] warning" <put user mail user>
    }

    for home in "${SYNC_ROOT_STORES[@]}"; do
        user="$(echo "$home" | cut -d'/' -f2)"
        user_db="$(get_database_name "$user")"

        timed_retention "$user" "$home" "$user_db"
    done

This script will take in a `DELETE` parameter to specify whether entries should
be deleted or not. Moreover, you can modify the script to add automated mail
send to specific addresses, for instance to each user to warn them of their
expired files, or to other addresses for monitoring.

Scripts functionning
--------------------

The scripts work in two-fold:
  - first the sync script is used to populate one database per user. This
database will contain all entries in specific directories, and calculate their
expiration date if the retention attribute is set.
  - second the update-retention script will use the database created by the sync
script to check for expired directories, and perform the logic explained in the
`Directory Retention Logic`__ section.

Crontab
=======

The two scripts above can be automatically started using the crontab utility.
For instance, you can have the following:

.. code:: sh

    # Run the sync script everyday at 8PM
    0 20 * * * retention_sync_script.sh
    # Run the update-retention script everyday at 10PM
    0 22 * * * retention_update_script.sh

Monitoring
==========

To monitor the scripts and verify their proper execution, the first place to
look at are the scripts' logs. For each user, two log files are generated when
the scripts are run. The first one, `retention-sync-<user>-<date>.log` shows
the execution of the sync script, especially the time taken and the logs of the
`rbh-sync` command execution. The second one,
`retention-update-<user>-<date>.log` shows similar information to the sync, but
for the update script, which wraps the `rbh-update-retention` command.

Troubleshoot
============

Mongo issues
------------

If the logs detail an issue with the Mongo database, the first thing to check
is the MongoDB daemon:

.. code:: sh

    systemctl status mongod

If it is marked failed or inactive without errors, that means the MongoDB daemon
is not started, in which case a `systemctl start mongod` does the job.

There might also be errors regarding the Mongo directories not existing, which
can be solved with a:

.. code:: sh

    mkdir -p  /var/run/mongodb/

If the issue is about the PID file of the process, similarly, do a:

.. code:: sh

    touch /var/run/mongodb/mongod.pid

There might also be issues with the daemon not having access rights to this
directory and file, in which case a `chown` will solve the issue:

.. code:: sh

    chown -R mongod:mongod /var/run/mongodb/

RobinHood 4 issues
------------------

There are different kinds of RobinHood 4 issues that can appear. They are given
by the commands done in the two scripts.

`Unable to load backend plugin: librbh-<any library>.so: cannot open shared object file: <error>`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If a RobinHood 4 command outputs the above error, that means the backend
library used by the command cannot be opened.

If the error is that there is `No such file or directory`, that means the
library isn't in a standard library path (`/usr/lib64` and such) or in the
`LD_LIBRARY_PATH` environment variable. To solve this issue, locate the
requested library, and either move it to standard path, or add its location to
the `LD_LIBRARY_PATH` environement variable.

If the error is `Permission denied`, that means the user running the command
cannot open the library. This can be solved with a `chmod` on the library.
