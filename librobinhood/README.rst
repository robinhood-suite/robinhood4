.. This file is part of RobinHood 4
   Copyright (C) 2020 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#####################
The RobinHood library
#####################

The RobinHood library is the heart of the RobinHood project (since version 4).
It provides an efficient C-API to store and query any filesystem's metadata.

A broad description of its internals is available here__.

.. __: https://github.com/robinhood-suite/robinhood4/blob/main/librobinhood/doc/internals.rst

Documentation
=============

To build the API documentation, use doxygen:

.. code:: bash

    # Generate a template configuration file for doxygen (Doxyfile)
    doxygen -g

    # Change some of the default parameters:
    doxyconfig() {
        sed -i "s|\($1\s*\)=.*$|\1= ${*:2}|" Doxyfile
    }
    doxyconfig PROJECT_NAME      librobinhood
    doxyconfig INPUT             include/robinhood
    doxyconfig RECURSIVE         YES
    doxyconfig FILE_PATTERNS     "*.h" "*.h.in"
    doxyconfig EXTENSION_MAPPING in=C
    doxyconfig EXTRACT_ALL       YES
    doxyconfig EXTRACT_STATIC    YES
    doxyconfig ALIASES           error=@exception

    # Run doxygen
    doxygen

Usage
=====

The following program, linked with the RobinHood library, will print the name of
every entry under the current directory and exit:

.. code:: C

    #include <errno.h>
    #include <error.h>
    #include <stdio.h>
    #include <stdlib.h>

    #include <robinhood.h>
    #include <robinhood/utils.h>

    static const char *uri = "rbh:posix:.";

    int
    main()
    {
        struct rbh_filter_options options = {
            .projection = {
                .fsentry_mask = RBH_FP_NAME,
            }
        };
        struct rbh_mut_iterator *fsentries;
        struct rbh_backend *backend;

        /* Build a backend from a URI */
        backend = rbh_backend_from_uri(uri);

        /* Fetch every fsentry */
        fsentries = rbh_backend_filter(backend, NULL, &options);

        /* Iterate over each fsentry and print its name (if it is set) */
        while (1) {
            struct rbh_fsentry *fsentry;

            errno = 0;
            fsentry = rbh_mut_iter_next(fsentries);
            if (fsentry == NULL)
                break;

            /* Some fsentries may not have a name in the backend */
            if (fsentry->mask & RBH_FP_NAME)
                printf("%s\n", fsentry->name);
        }

        if (errno != ENODATA)
            error(EXIT_FAILURE, errno, "while iterating over %s", uri);

        rbh_mut_iter_destroy(fsentries);
        rbh_backend_destroy(backend);

        return EXIT_SUCCESS;
    }

For more advanced use cases, check out the following applications built on top
of librobinhood:
 - rbh-sync_ to synchronize two backends
 - rbh-fsevents_ to update a backend with changelog events
 - rbh-find_ to query a backend and filter entries

.. _rbh-sync: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-sync
.. _rbh-fsevents: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-fsevents
.. _rbh-find: https://github.com/robinhood-suite/robinhood4/tree/main/rbh-find

Remote MongoDB Database
=======================

Librobinhood can use a remote mongo database for the mongo backend. To do this,
you must define the environment variable ``RBH_MONGODB_ADDRESS`` with the host
and port where the database is located.

The variable must follow the form ``mongodb://HOST:PORT``

.. code:: bash

    export RBH_MONGODB_ADDRESS=mongodb://localhost:27017

Limits
======

Maximum File Path Length
------------------------

RobinHood4, using the basic traverser FTS_, can theoretically handle paths up to
USHRT_MAX (65,535 characters). However, testing shows that the actual maximum
path length falls between 36,000 and 38,000 characters, depending on the size of
directory names in the tree.

To minimize frequent memory allocations, FTS_ allocates slightly more than twice
the space needed to store the last discovered path. The formula used to
determine the new required size is:

max_path = max_path + strlen(new_path) + 256 + 1, with max_path initially set to
MAXPATHLEN.

If a checked path is too long, FTS_ returns an error, causing the application to
stop. Using the --no-skip flag prevents the application from stopping with an
error, but it will still terminate as if there were no more entries to process.

With the mpiFileUtils traverser, RobinHood4 supports paths up to 4,096
characters. If a checked path exceeds this limit, an error is printed, and
mpiFileUtils skips the path. Since mpiFileUtils automatically skips too long
paths, using the --no-skip flag does not cause the application to stop.

Known Issues
============

There is currently a known issue with rbh-sync_ where during each
synchronization using the basic traverser FTS_, the `access time` of directories
is updated after each pass. This is due to the traverser FTS_ opening
directories for traversal without a `O_NOATIME` flag.

A workaround for this issue to use a system mounted with the `noatime` or
`nodiratime` (see `mount`'s man page for more information).

This issue is also present with the MPI_ traverser, but a patch has already been
integrated to solve it. A fix to use this update will be integrated to
the MPI backends later.

.. _FTS: https://man7.org/linux/man-pages/man3/fts.3.html
