RobinHood4
==========

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
