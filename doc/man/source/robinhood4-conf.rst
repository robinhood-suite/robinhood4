RobinHood4(5)
=============

DEFAULT CONFIGURATION
---------------------

With your installation of RobinHood4, a default configuration file is
placed at `/etc/robinhood4.d/default.conf`. All the RobinHood4 commands
implement the `--config` option to override the configuration used.

Bellow are detailed the different values that can be set in the configuration
file. The default values and typing are available in the default configuration
file.

BACKEND
-------

While URIs can use plugin currently installed, these can be improved with
extensions. To indicate which combination of plugin/extensions are available,
you can specify their names in the `backends` section.

Each combination must be composed of the `extends` key with a string as value
corresponding to a currently installed plugin. Then, you can define additional
attributes to modify the behaviour of the extended plugin. Only that plugin will
read those additionnal keys and handle them.

For instance, a backend extending the POSIX plugin can only define an iterator
to use and enrichers with which to retrieve additional data.

.. code:: yaml

    backends:
        posix-mpi:
            extends: posix
            iterator: mfu

Here, we define the `posix-mpi` backend, which extends the `posix` plugin and
will use the `mfu` extension. It will be usable with the `rbh:posix-mpi:/tmp`
URI for instance.

Another example in the default configuration file:

.. code:: yaml

    backends:
        lustre-mpi:
            extends: posix
            iterator: mfu
            enrichers:
                - lustre

Here, we define the lustre-mpi backend, meaning `rbh:lustre-mpi:/mnt/lustre` is
now a valid URI. This backend:
 - extends the POSIX plugin, meaning the backend works like it and retrieves
   the same information
 - iterate with the MFU iterator, meaning the backend will go through each
   entry in the mount point with mpiFileUtils (instead of the default FTS
   algorithm)
 - enrich each entry with Lustre information, meaning the backend will use the
   Lustre extension to retrieve additional information about each entry

An important thing to note is that the name of a backend can be the same as the
name of a plugin or extension. In that case, the backend will take priority
over the extension/plugin.

For instance:

.. code:: yaml

    backends:
        posix:
            extends: posix
            iterator: mfu
            enrichers:
                - retention
                - lustre

Is a valid backend, and means using the URI rbh:posix:/mnt/lustre will iterate
over each entry in /mnt/lustre with the MFU iterator and use the
Lustre/retention extensions. Moreover, just using the POSIX plugin will NOT be
doable anymore.

ALIAS
-----

Aliases are a way to shorten frequently used command lines. All commands
understand aliases, and they must be defined in the configuration file as a
dictionnary in the `alias` section, for instance:

.. code:: yaml

    alias:
        blob1: "-name blob -printf '%A'"

Here is defined the `blob1` alias, which makes the following lines equal:

.. code:: bash

    $ rbh-find rbh:mongo:blob --alias blob1
    $ rbh-find rbh:mongo:blob -name blob -printf '%A'

EXTENDED ATTRIBUTE MAP
----------------------

By default, extended attributes are read as binary values, which is most likely
not the typing you want for it. Therefore, you can define the `xattrs_map`
section as a dictionnary with each key the extended attribute name, and the
value the typing associated with that name. The types can be: int32, int64,
uint32, uint64, string, boolean and binary.

Here is an example of the map:

.. code:: yaml

    xattrs_map:
        user.blob_int32: int32
        user.blob_string: string
        user.blob_boolean: boolean

PLUGIN-SPECIFIC ATTRIBUTES
--------------------------

MONGO
+++++

You can define Mongo specific attributes in the `mongo` section of the
configuration file. These attributes can be:
 - `address`: the connection string to connect to the Mongo database and how
 - `cursor_timeout`: the timeout of the libmongoc cursor

RETENTION
+++++++++

You can define the extended attribute used to set the retention date of an entry
with the `RBH_RETENTION_XATTR` key.

SEE ALSO
--------

robinhood4(7)

CREDITS
-------

RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
