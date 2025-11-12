rbh-fsevents(1)
===============

NAME
----

rbh-fsevents - Process and manage filesystem events (fsevents) with Robinhood 4

SYNOPSIS
--------

**rbh-fsevents** [**-h** | **--help**] [**--version**] [**--verbose**] [**--raw**] [**--workers** *N*] [**--enrich** *MOUNTPOINT*] *SOURCE* *DESTINATION*

DESCRIPTION
-----------

`rbh-fsevents` is a command-line tool in the Robinhood 4 suite for collecting filesystem events (fsevents), optionally enriching them with additional metadata, and sending the processed events to a backend system or other destination.

Its primary use is to monitor and process large-scale filesystems. Common tasks include tracking file creations, deletions, metadata updates, and applying these updates to storage backends such as MongoDB.

OPTIONS
-------

POSitional ARGUMENTS
+++++++++++++++++++++

**SOURCE** [Required]
    Specifies the event source. Supported options include:
    - A path to a YAML configuration file, or `-` for standard input (stdin).
    - The Metadata Target (MDT) name from a Lustre filesystem (e.g., `lustre-MDT0000`).

**DESTINATION** [Required]
    Specifies the event destination. Supported options include:
    - `-` for standard output (stdout).
    - A URI for storage backends, such as MongoDB (e.g., `rbh:mongo:test`).

OPTIONAL ARGUMENTS
++++++++++++++++++

**-h**, **--help**
    Displays this help message and exits.

**--version**
    Displays the current version information for the tool and exits.

**--verbose**
    Runs the tool in verbose mode, displaying additional information about processing.

**--raw**
    Outputs raw fsevents as they are collected, without enrichment. This mode disables all enrichment functionality.

**--enrich** *MOUNTPOINT*
    Enriches the collected events by querying additional metadata from the specified *MOUNTPOINT* (e.g., `/mnt/data`). Enrichment adds filesystem-specific details to events, allowing better integration with backends.

**--workers** *N*
    Specifies the number of worker threads to use during event processing. This increases parallelism for processing large volumes of events. For example, `--workers 4` enables four worker threads.

USAGE EXAMPLES
--------------

### Combined Workflow: Collect, Enrich, and Update in One Command

The most streamlined way to use `rbh-fsevents` is to collect filesystem events, enrich them, and update a backend in a single command:

```console
rbh-fsevents --enrich /mnt/scratch lustre-MDT0000 rbh:mongo:test
```

- **SOURCE**: `lustre-MDT0000` represents an MDT from which events are collected.
- **--enrich /mnt/scratch**: Specifies a mount point for additional metadata enrichment.
- **DESTINATION**: `rbh:mongo:test` sends the processed and enriched events directly to a MongoDB backend.

### Separate Workflow: Step-by-Step Processing

#### Step 1: Collect Sparse Events
Collect filesystem events from an MDT and save them as sparse events in a YAML file:
```console
rbh-fsevents lustre-MDT0000 - > /tmp/sparse-fsevents.yaml
```

#### Step 2: Enrich Sparse Events
Read the sparse events from the previously collected file, enrich them with additional metadata from a specified mount point, and output the enriched events:
```console
rbh-fsevents --enrich /mnt/data - < /tmp/sparse-fsevents.yaml > /tmp/enriched-fsevents.yaml
```

#### Step 3: Update Backend with Enriched Events
Send the enriched events to a Robinhood-compatible backend, such as MongoDB:
```console
rbh-fsevents - < /tmp/enriched-fsevents.yaml rbh:mongo:test
```

NOTES
-----

- Raw fsevents cannot be directly uploaded to a Robinhood backend. They must be enriched first.
- Adjust the number of worker threads for performance tuning using the `--workers` option.
- Verbose mode (`--verbose`) can provide debugging or log information helpful for monitoring the program's state and progress while processing events.
- **Daemon Support**: A future update to `rbh-fsevents` is expected to include support for running as a daemon, allowing continuous monitoring and real-time processing of filesystem events.
