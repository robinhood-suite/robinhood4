.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

####################
Userland RobinHood 4
####################

This design document pertains to the use of the RobinHood 4 suite in userland.
The goal is to provide to regular users some of RobinHood 4's tools so that
they themselves can gather and retrieve statistics they deem relevant about
their usage of a system.

Current behaviour
=================

Currently, the RobinHood 4 suite is only usable by administrators. There are
multiple reasons for this:
 * some tools require privilege access. For instance, `rbh-fsevents` requires
   access to an MDT's changelogs.
 * some backends require privilege access to operate properly. For instance, the
   Mongo backend requires access to the Mongo database.
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
attribute requiring elevated access.

Moreover, regular users have no need for these tools, therefore, having access
to `rbh-sync`, `rbh-find` and `rbh-report` is enough.

Backends
--------

To allow users to use specific backends, especially the Mongo one, any user must
be able to use a Mongo database. This can work well for user-created databases,
which they could then search by themselves, and be assured their sole access to.

Therefore, as long as regular users can create, fill and check their own
databases, there is no issue. However, regular users must **not** be able to
access the main database, as it would allow them to see every data stored and
change them. The rest of this document will focus solely on the main mirror
system, and not user-created ones.

Information Boundaries
----------------------

The third issue to solve is to prevent a user A from accessing information
about a user B, and while it is an issue for the mirror systems, especially the
Mongo database, it is not an issue for the filesystem backends, as those use the
system itself to prevent illegal access of information between users.

Thus, we only need to fix the issue for main mirror systems (i.e. mirror of the
whole filesystems) and access to their content. This can be done by wrapping
calls to `rbh-find` to use the user's UID and GID, so that can only target
their own data in a mirror system.

To do this, we must check a user's credentials when doing the command, and
adding the filters for UID and GID automatically. However, since the main
database can't be given free access to regular users, it is necessary to pass
these requests in a centralized point which can then access the main database,
for instance a remote server.

That server would thus receive filtering requests by users, retrieve their
UID and GID, and issue the proper `rbh-find` command according to the user's
information and request.

Security
--------

Since there is no need for the remote server to modify the main database, we do
not have to enforce security beyond ensuring users can't see data of other
users. Therefore, we can use standard authentication processes, like Kerberos
ticketing. With this, a user's identity could be ensured, and using the
enforced filtering on the UID and GID, we could prevent any data leak or
change.


Implementation
==============

Remote server
-------------

All in all, to allow regular users to use the RobinHood 4 suite (not including
creating and filtering mirror systems of their own creation), we must allow
them authenticated access to the main mirror system through a remote server.
In the rest of this document, we will consider Mongo the mirror system, but
the implementation could be adapted to other backends.

This server will be based on the one done in the IO-SEA_ project to handle
HSM requests, the `Ganesha Request Handler`__.

.. _IO-SEA: https://iosea-project.eu/
__ https://github.com/io-sea/GRH

This is a remote server written in Python which can be used as an Apache_
server, authenticated with Kerberos_, and will use Celery_ to handle requests.

.. _Apache: https://httpd.apache.org/
.. _Kerberos: https://web.mit.edu/kerberos/
.. _Celery: https://docs.celeryq.dev/en/stable/

Since this is a remote server, the Apache front and Celery workers would ensure
large amounts of concurrent requests can be received and handled. Moreover,
users could send their `rbh-find` and `rbh-report` requests alongside their
Kerberos ticket, and the server would enforce the UID and GID filter on those
same requests, to only retrieve the user's data.

There will be four checkpoints for the requests:
 * the user (or client in this context) will issue a `rbh-find` or `rbh-report`
   request to the server, which may contain several filters and actions,
   alongside their Kerberos ticket.
 * the server will receive the request and dispatch it to a Celery worker.
 * the worker will start a corresponding `rbh-find` or `rbh-report` call of its
   own, targeting the main mirror system.
 * the client will then ask the result of the request, possibly multiple times,
   to retrieve every information obtained if necessary.

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
     more like options of `rbh-find` and `rbh-report` than anything (we cannot
     implement options the basic tools as we have no way of enforcing users to
     set these options).
 * Second option:
   * Pros: only the backend has to be developed, and the filter parsing/creation
     is handled by `rbh-find` and `rbh-report`.
   * Cons: Conversion of the filters to C structures would be done in the two
     tools, then converted to string to be sent in an HTTP request, then
     converted back to C structures for the actual filtering.

We decided to go with the second option, as we can alleviate the double
conversion issue by directly giving the backend the command line content, not
just the converted structures. And even if that is not possible or too
intrusive of a change, the time taken for the double conversion is negligeable
compared to the actual requests times, and we wouldn't have two more "dummy"
tools.

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
=======

In the end, the regular users will be able to call to do the following commands:

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
