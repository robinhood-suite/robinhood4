.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

####################
Userland RobinHood 4
####################

This design document pertains to the use of the RobinHood 4 suite in by
regulars users. The goal is to provide them with some of RobinHood 4's tools so
that they themselves can gather and retrieve statistics they deem relevant
about their usage of a system.

Current behaviour
=================

Currently, the RobinHood 4 suite is only usable by administrators. There are
multiple reasons for this:
 * some tools require privileged access. For instance, `rbh-fsevents` requires
   access to an MDT's changelogs.
 * some backends require privileged access to operate properly.
 * information available in a mirror system about one user shouldn't be
   available to all other users, for privacy or classification purposes.
 * regular users shouldn't be able to tamper with the content of a mirror
   system, to prevent security issues.


In order to allow users access to some of RobinHood 4's features and
information, we need to address these reasons and either modify existing tools
and backends, or provide new tools or backends.

Changes proposed
================

Tools
-----

Regarding tools that require elevated access, they mainly either pertain to
their basic requirements. For instance, `rbh-fsevents` and
`rbh-update-retention` cannot be used by regular users as the first one
**requires** access to an MDT, and the second needs to set an extended
attribute in the trusted namespace.

Moreover, regular users have no need for these tools, therefore, having access
to `rbh-find` and `rbh-report` is enough.

Information Boundaries and Security
-----------------------------------

Since there is no need for regular users to modify the main mirror system, they
should only be provided read access to it. Moreover, we want to prevent a user
A from accessing information about a user B, which can be enforced by using
standard POSIX access rules.

This can be done by wrapping calls to `rbh-find` to use the user's UID and GID,
so that can only target their own data in a mirror system. To do this, we must
check a user's credentials when doing the command, and add the filters for UID
and GID automatically. We also need to account for the entries' mode in the
database, but this is only a combination of filters for permissions given a
particular UID and GID.

The retrieval of a user's credentials may be done with standard authentication
protocols, like Kerberos ticketing. With this, a ticket will give the user's
exact username, annd then we can for instance use LDAP to retrieve the UID and
GID, filter entries based on this UID and GID, and prevent any data leak or
change.

Implementation
==============

Remote server
-------------

All in all, to allow regular users to use the RobinHood 4 suite (not including
creating and filtering mirror systems of their own creation), we must allow
them authenticated access to the main mirror system, for instance through a
remote server. In the rest of this document, we will consider Mongo the mirror
system, but the implementation could be adapted to other backends.

This server will be similar [#]_ to the one done in the IO-SEA_ project to
handle HSM requests, the `Ganesha Request Handler`__.

.. _IO-SEA: https://iosea-project.eu/
__ https://github.com/io-sea/GRH

This is a remote server written in Python which can be used as an Apache_
server and authenticated with Kerberos_.

.. _Apache: https://httpd.apache.org/
.. _Kerberos: https://web.mit.edu/kerberos/

Since this is a remote server, the Apache frontend would ensure large amounts of
concurrent requests can be received and handled. Moreover, users could send
their `rbh-find` and `rbh-report` requests alongside their Kerberos ticket, and
the server would enforce the UID and GID filter on those same requests, to only
retrieve the user's data.

There will be four checkpoints for the requests:
 * the user (or client in this context) will issue a `rbh-find` or `rbh-report`
   request to the server, which may contain several filters, alongside their
   Kerberos ticket.
 * the server will spawn a thread to handle the request.
 * the worker will start a corresponding `rbh-find` or `rbh-report` call of its
   own, targeting the main mirror system.
 * the client will then ask the result of the request, to retrieve every
   information obtained if necessary.

.. [#] depending on our needs and timeline, we may either start from a smaller
       version of the Ganesha Request Handler, or we may start a server from
       scratch, as the tool has deduplication and Celery components that are
       unnecessary for our use case.

HTTP backend
------------

To access information, the user must issue `rbh-find` and `rbh-report` commands.
However, since they cannot target the main database directly, they must target
the remote server. We therefore have two options: either create a
`rbh-find`/`rbh-report`-like tool to get the credentials, and send the request
to the server, or create a new backend that will do the same job.

Both approaches have their pros and cons:
 * First option:
   * Pros: Send the command line request as-is to the server, thus not having
     twice the conversion to a `rbh_filter` structure.
   * Cons: Creating a simple overlay of the tools, adding two new tools that are
     more like options of `rbh-find` and `rbh-report` than anything.
 * Second option:
   * Pros: only the backend has to be developed, and the filter parsing/creation
     is handled by `rbh-find` and `rbh-report`.
   * Cons: the http backend on the client command has to convert the rbh_filter
     to an HTTP request. Then the HTTP server has to convert the HTTP request
     to an rbh_filter again.

We decided to go with the second option, as we can alleviate the double
conversion issue by directly giving the backend the command line content, not
just the converted structures.

Then, when sent to the remote server, the HTTP request would look like this:

.. code:: Bash

    rbh-find rbh:http:<distant_server> -size +3G -type f

    POST /find/mongo/<dbname>
    Body:
        size: "+3G"
        type: "f"

Finally, this backend will only be usable for filtering, as we have no need to
update it, since that will be done by admins directly with the Mongo backend.
We therefore only need to implement the `rbh_backend_filter` function.

Its role will be to:
 * get the requested filters from `rbh-find` and `rbh-report`
 * retrieve the user's credentials
 * convert both filters and credentials into an HTTP request
 * send the request to the HTTP server
 * status regularly to get the request's results
 * display the results

Example
~~~~~~~

In the end, the regular users will be able to do the following commands:

.. code:: Bash

    rbh-find rbh:http:<distant_server> -size +3G -type f
    /some_file_of_size_5G
    /some_file_of_size_2T

    export rbh_remote_server_address="<remote_address>"
    rbh-report rbh:http: -group-by "statx.type" -output "count()"
    file: 1337
    dir: 42

Of course, the remote server's address will also be obtainable from the
default configuration file, so users won't have to know it themselves.
