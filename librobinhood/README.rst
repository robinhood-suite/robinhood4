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

Installation
============

Build and install with meson_ and ninja_:

.. code:: bash

    cd robinhood4/librobinhood
    meson builddir
    ninja -C builddir
    sudo ninja -C builddir install

.. _meson: https://mesonbuild.com
.. _ninja: https://ninja-build.org

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
you must define the environment variable ``RBH_MONGO_DB_URI`` with the host
and port where the database is located.

The variable must follow the form ``mongodb://HOST:PORT``

.. code:: bash

    export RBH_MONGO_DB_URI=mongodb://localhost:27017
