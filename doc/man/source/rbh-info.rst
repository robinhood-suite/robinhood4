rbh-info
========

rbh-info(1)
===========

NAME
----

rbh-info - Display information about Robinhood backends and their metadata

SYNOPSIS
--------

**rbh-info** [**-h** | **--help**] [**-l** | **--list**] *[PLUGIN/URI]* [**POST_URI_OPTIONS**]

DESCRIPTION
-----------

The `rbh-info` tool provides detailed insights about installed Robinhood backends, their capabilities, and metadata related to synchronization or entries collections.

This tool helps administrators and advanced users query backend-specific metrics, such as storage capacities, synchronization states, and collection statistics.

OPTIONS
-------

General Options
+++++++++++++++

**-h**, **--help**
    Displays this help message and exits.

**-l**, **--list**
    Lists all available backends for Robinhood.

Post-URI Options
++++++++++++++++

**-a**, **--avg-obj-size**
    Displays the average size of objects in the entries collection of the backend.

**-b**, **--backend-source**
    Shows which source backend was used in past synchronizations.

**-c**, **--count**
    Shows the count of documents in the entries collection of the backend.

**-s**, **--size**
    Displays the total size of the entries collection in a backend.

**-f**, **--first-sync**
    Shows details about the first synchronization performed.

**-y**, **--last-sync**
    Displays the last synchronization's metadata.

USAGE EXAMPLES
--------------

### List Installed Backends
```bash
rbh-info -l
```

### Display Capabilities of the Mongo Plugin
```bash
rbh-info mongo
```

### Query Metadata for a Specific Backend
```bash
rbh-info rbh:mongo:test -c -s
```
This retrieves the count of documents and total size for the backend `test` configured with MongoDB.

NOTES
-----

- Outputs may vary based on the backend plugin in use.
- Queries are performed against configured backends and require that these backends provide specific capabilities (e.g., average size, synchronization metadata).
