rbh-fsevents(1)
===============

NAME
----

rbh-fsevents - Process and manage storage system events

SYNOPSIS
--------

**rbh-fsevents** *SOURCE* *DESTINATION* [**OPTIONS**]

DESCRIPTION
-----------

Collect SOURCE system events (fsevents), optionally enrich them with additional
metadata, and store the processed events in DESTINATION.

ARGUMENTS
---------

PARAMETERS
++++++++++

**SOURCE**
    Specifies the event source. Supported formats include:
    - A path to a YAML configuration file
    - `-` for standard input (stdin)
    - A URI with the `src` schema and where to read events, for instance The
      Metadata Target (MDT) name from a Lustre filesystem (e.g.,
      `lustre-MDT0000`).

**DESTINATION**
    Specifies the event destination. Supported formats include:
    - `-` for standard output (stdout).
    - A URI for storage backends, such as MongoDB (e.g., `rbh:mongo:test`).

OPTIONS
+++++++

**--alias** *NAME*
    Specify an alias for the operation. Aliases are a way to shorten the
    command line by specifying frequently used components in the configuration
    file under an alias name, and using that alias name in the command line
    instead.

**-b**, **--batch-size** *N*
    Specify the number of entries collected to keep in memory for deduplication
    process (100 by default).

**-c**, **--config** *PATH*
    Specify the configuration file to use.

**--dry-run**
    Display the command after alias management.

**-d**, **--dump** *PATH*
    Specify the path to a file where the changelogs should be dumped, or `-` for
    stdout, can only be used with a Lustre source.

**-e**, **--enrich** *URI*
    Enriches the collected events by querying additional metadata from the
    specified URI (e.g., `rbh:lustre:/mnt/lustre`).

**-h**, **--help**
    Displays the help message and exits.

**-m**, **--max** *N*
    Specify the maximum number of events to read.

**-r**, **--raw**
    Outputs raw fsevents as they are collected, without enrichment (default
    behaviour). This mode disables all enrichment functionality.

**-v**, **--verbose**
    Runs the tool in verbose mode, displaying additional information about
    processing.

**-w**, **--nb-workers** *N*
    Specifies the number of worker threads to use during event processing and
    update.

EXAMPLES
--------

### Combined Workflow: Collect, Enrich, and Update in One Command

The most streamlined way to use `rbh-fsevents` is to collect events, enrich
them, and update a backend in a single command:

```bash
rbh-fsevents --enrich /mnt/scratch src:lustre:lustre-MDT0000 rbh:mongo:test
```

- **SOURCE**: `src:lustre:lustre-MDT0000` represents an MDT from which events
  are collected
- **--enrich rbh:lustre:/mnt/scratch**: Specifies a mount point for additional
  metadata enrichment.
- **DESTINATION**: `rbh:mongo:test` sends the processed and enriched events
  directly to a MongoDB backend.

### Separate Workflow: Step-by-Step Processing

#### Step 1: Collect Sparse Events
Collect filesystem events from an MDT and save them as sparse events in a YAML
file:
```bash
rbh-fsevents src:lustre:lustre-MDT0000 - > /tmp/sparse-fsevents.yaml
```

#### Step 2: Enrich Sparse Events
Read the sparse events from the previously collected file, enrich them with
additional metadata from a specified mount point, and output the enriched
events to another file:
```bash
rbh-fsevents --enrich rbh:lustre:/mnt/data - - < /tmp/sparse-fsevents.yaml > /tmp/enriched-fsevents.yaml
```

#### Step 3: Update Backend with Enriched Events
Send the enriched events to a backend, such as MongoDB:
```bash
rbh-fsevents - rbh:mongo:test < /tmp/enriched-fsevents.yaml
```

NOTES
-----

- Raw fsevents cannot be directly uploaded to a Robinhood backend. They must be
  enriched first.
- **Daemon Support**: A future update to `rbh-fsevents` is expected to include
  support for running as a daemon, allowing continuous monitoring and real-time
  processing of storage system events.

SEE ALSO
--------

rbh4(5), rbh4(7)

CREDITS
-------

rbh-fsevents and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
------

rbh-fsevents and RobinHood4 is written by the storage software development team
of the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
