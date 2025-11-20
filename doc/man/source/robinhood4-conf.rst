RobinHood4(5)
=============

Here are detailed the different values that can be configured in the
configuration file.

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

Using the Configuration File, you can define additional backends that will be
usable in URIs. Those backends will extend a base plugin, which means they will
perform the same operations as that plugin. Moreover, each plugin can define a
set of attributes that only it can understand and will change the behaviour of
that backend. For instance, a backend extending the POSIX plugin can only
define an iterator to use and enrichers with which to retrieve additional data.

An example of this is shown in the default configuration file:

backends:
    lustre-mpi:
        extends: posix
        iterator: mfu
        enrichers:
            - lustre

Here, we define the lustre-mpi backend, meaning rbh:lustre-mpi:/mnt/lustre is
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


