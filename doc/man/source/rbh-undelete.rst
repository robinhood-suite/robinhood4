rbh-undelete
============

rbh-undelete(1)
===============

NAME
----

rbh-undelete - Restore deleted files or objects using archived metadata in Robinhood backends

SYNOPSIS
--------

**rbh-undelete** [**-h** | **--help**] [**-c CONFIG**] [**-l** | **--list** | **--restore**] [**--output PATH**] SOURCE DEST

DESCRIPTION
-----------

The `rbh-undelete` tool is part of the Robinhood suite, designed for recovering deleted files or objects that were previously archived. The tool queries metadata stored in Robinhood backends to recreate entries in their original or specified paths.

OPTIONS
-------

**General Options**
+++++++++++++++++++

**-h, --help**
    Displays this help message and exits.

**-c, --config PATH**
    Specifies the configuration file to use.

**List Options**
++++++++++++++++

**-l, --list**
    Lists all archived entries and their versions in the specified backend.

**Restore Options**
+++++++++++++++++++

**-r, --restore**
    Restores a deleted entry using metadata in the specified backend.

**--output OUTPUT**
    Specifies the path to recreate the restored file. This option must be used with `--restore`.

USAGE EXAMPLES
--------------

1. **List Deleted Entries**
   ```bash
   rbh-undelete rbh:mongo:blob --list /mnt/lustre/
   ```

2. **Restore the Latest Deleted Version**
   ```bash
   rbh-undelete rbh:mongo:test rbh:lustre:/mnt/lustre/blob/foo/test3.c --restore
   ```

3. **Restore a File to a Custom Path**
   ```bash
   rbh-undelete rbh:mongo:test --restore --output /tmp/recovered/blob/foo/test3.c
   ```

NOTES
-----

- The `RBH_SCHEME` identifier is used to define backends, such as `mongo`, `lustre`, or `posix`.
- The tool ensures generic functionality to support multiple writeable backends.

