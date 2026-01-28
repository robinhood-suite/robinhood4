rbh-info(1)
===========

SYNOPSIS
--------

**rbh-info** [**GENERAL-OPTIONS**] [**-l** | **--list**]
**rbh-info** [**GENERAL-OPTIONS**] *PLUGIN*
**rbh-info** [**GENERAL-OPTIONS**] *URI* [**OPTIONS**]

DESCRIPTION
-----------

Provide information about currently-installed and usable Robinhood backends,
their capabilities, and metadata related to a URI.

3 modes are usable:
 - list mode, using **--list**, to list the currently installed RobinHood4
   plugins and extensions
 - capabilities mode, using only the name of a plugin, to get the capabilities
   of a plugin, i.e. by which tool can the plugin be used and how
 - info mode, using a valid URI, to get various information and statistics
   specified by the **OPTIONS**

ARGUMENTS
---------

GENERAL OPTIONS
+++++++++++++++

**-h**, **--help**
    Displays the help message and exits.

LIST MODE
+++++++++

**-l**, **--list**
    List the RobinHood4 plugins and extensions currently installed and usable.
    This corresponds to the plugins and extensions that can be used with
    dlopen(3).

CAPABILITIES MODE
+++++++++++++++++

**PLUGIN**
    Specify the plugin to get the capabilities of. They indicate which tool can
    be used with this plugin and in which direction

INFO MODE
+++++++++

**URI**
    Specify the URI from which to fetch information. These information
    correspond to various statistics and metadata about the URI and underlying
    storage component.

**-a**, **--avg-obj-size**
    Displays the average size of entries in the backend (as written on disk).

**-b**, **--backend-source**
    Shows which source backend was used in past synchronizations.

**-c**, **--count**
    Shows the number of entries in the backend.

**-f**, **--first-sync**
    Shows details about the first synchronization performed in that backend.
    These include: duration of the sync, when it started and when it ended (as
    timestamps), mountpoint used for the sync, the complete command line for
    that sync, and the number of entries seen, converted and skipped.

**-s**, **--size**
    Displays the total size of the entries in a backend (as written on disk).

**-y**, **--last-sync**
    Shows details about the last synchronization performed in that backend. Same
    information as **--first-sync**.

EXAMPLES
--------

List Installed Backends
    | $ rbh-info --list
    | List of installed plugins and their extensions:
    | - mongo
    | - posix
    |     - retention

Display Capabilities of the Mongo Plugin
    | $ rbh-info mongo
    | Capabilities of mongo:
    | - filter: rbh-find [source]
    | - synchronisation: rbh-sync [source]
    | - update: rbh-sync [target]
    | - branch: rbh-sync [source for partial processing]

Query Metadata for a Specific Backend
    | $ rbh-info rbh:mongo:test --count --size
    | 6577
    | 3.58 MB

This retrieves the count of documents and total size for the backend `test`
configured with MongoDB.

NOTES
-----

- Queries are performed against configured backends and require that these
  backends provide specific capabilities (e.g. average size, synchronization
  metadata, ...).

SEE ALSO
--------

dlopen(3), robinhood4(5), robinhood4(7)

CREDITS
-------

rbh-info and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
