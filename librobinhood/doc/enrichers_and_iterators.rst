.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

###################################
Iterators and Enrichers refactoring
###################################

Context
=======

The actions performed by a POSIX based backend can be decomposed in two main
steps:

#. traverse the filesystem to build a stream of fsentries;
#. for each fsentry, enrich it with backend specific metadata.

The first step will be referred to as the **iteration** performed by an
**iterator** and the second step will be referred to as the **enrichment**
performed by an **enricher**.

**librobinhood** currently supports 4 POSIX like backends: **posix**,
**lustre**, **posix-mpi** and **lustre-mpi**. All these backends depend on the
**posix** backend to perform the POSIX enrichment and add additional functions
to perform backend specific enrichment. Furthermore, the MPI based backends
have their own iterator based on MPIFileUtils and the Lustre backend uses the
FTS iterator of POSIX.

The goal of this document is to suggest an approach that would decouple the
iterator and the enrichment from the backend itself so that we could simply mix
and match the functionalities that we want to build custom backends.

We can imagine several additional functionalities that would require new
backends with the current implementation. For instance:

* a single client multithreaded iterator;
* an SeLinux or POSIX ACL enricher.

Even if we implemented one new backend for each of these functionalities, we
would still need to build additional backends if we want to mix
functionalities (e.g. a Lustre backend with support for SeLinux and MPI).

In order to make **librobinhood** more flexible and easier to extend, we propose
to add the support for backend extensions.

Definition
==========

An extension is a loadable library that contains a specific symbol that
librobinhood will look for. For instance, the lustre extension to the
posix backend will be found in the library **librbh-posix-lustre-ext.so**
and contain the symbol **_RBH_POSIX_LUSTRE_PLUGIN_EXTENSION**. As for backends,
extensions will share a common base structure **struct rbh_plugin_extension**.
Librobinhood will only deal with this generic type. The actual type will
be defined by the backend that is extended. For instance, the posix backend
will provide a **struct rbh_posix_extension** that can be used by plugins
that wish to extend the posix backend.

To declare an extension:

.. code-block:: c

   #include "posix_extension.h"

   struct rbh_posix_extension RBH_BACKEND_EXTENDS(POSIX, LUSTRE) = {
   };

To load an extension:

.. code-block:: c

   #include "posix_extension.h"

   const struct rbh_posix_extension *extension;
   const struct rbh_backend_plugin *posix;

   posix = rbh_backend_plugin_import("posix");
   extension = rbh_plugin_load_extension(posix, "lustre");

Declaring custom backends
=========================

Since extensions are not actual backends, we need to provide a way to indicate
which functionalities we would like to use. This can be done through
configuration. We can reproduce the existing backends like this:

.. code-block:: yaml

   backends:
       lustre:
           extends: posix
           iterator: fts
           enrichers:
               - lustre
               - retention
       posix-mpi:
           extends: posix
           iterator: mfu
       lustre-mpi:
           extends: posix
           iterator: mfu
           enrichers:
               - lustre
               - retention

The idea here is that functions such as **backend_new** will now look into the
configuration if the name of the backend is declared under the **backends**
key. If not, it will try to load the backend given as an argument and return an
error if it does not exist or is not a robinhood backend.

If however, the name is declared in the configuration, the **extends** key will
indicate to librobinhood which plugin to load (**posix** in this example).
Once the plugin is loaded the ``new`` backend constructor of the posix backend
will once again look at the configuration to know which extensions it should
load.

In this example, for the Lustre backend it will look for the **fts** iterator
which will still be in the posix backend's source code and will therefore not
need to load an extension for it. Then under the **enrichers** key, it will find
a list of extensions to load (namely **lustre** and **retention** here).

Extensions can be used for iterator as well. This is the case for **lustre-mpi**
and **posix-mpi**. Since the POSIX backend only implements the FTS iterator,
if another name is given (e.g. **mfu**), the POSIX backend will try to load
the **mfu** extension.

The **iterator** and **enrichers** keys as well as the behavior they trigger are
specific to the POSIX backend. If other backends use extensions, they may need
different keys and have a different behavior. They will however use the
**extends** key.

Implementation
==============

To achieve this, we need to extract the retention logic outside of the Lustre
backend and move it to its own plugin. A new plugin with the MPIFileUtils
iterator has to be created as well. The lustre-mpi and posix-mpi backends can
be removed. The lustre backend can be simplified to only contain the enrichment
logic. Finally, the POSIX backend has to be extented to read the new
configuration and load all extensions accordingly.

``rbh-capabilities`` will have to be extended to list extensions as well.

Versioning
==========

Since extensions are specific plugins they will have a version stored in their
``rbh_plugin`` structure. An extension will be compatible with a given set of
versions of the plugin it extends. To implement this, a new structure
``rbh_plugin_extension`` will be created:

.. code-block:: c

   struct rbh_plugin_extension {
       const char *super;
       const char *name;
       uint64_t version;
       uint64_t min_version;
       uint64_t max_version;
   };

An extension will therefore support versions of the plugin it extends ranging
from ``min_version`` to ``max_version``. This check will be done when loading
the extension.
