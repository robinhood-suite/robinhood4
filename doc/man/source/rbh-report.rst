rbh-report
==========

rbh-report(1)
=============

NAME
----

rbh-report - Generate aggregated reports from Robinhood backends

SYNOPSIS
--------

**rbh-report** [**-h** | **--help**] [**-v** | **--verbose**] [**--csv**] [**--config PATH**] *SOURCE* [**--output OUTPUT**] [**--group-by FIELD**] [*PREDICATES*]

DESCRIPTION
-----------

`rbh-report` is a command-line tool for querying Robinhood-compatible backends and generating reports based on aggregated information. It utilizes backend filtering capabilities to summarize storage data and present it in a user-friendly format.

The tool is useful for understanding storage usage, examining file distributions, and generating usage statistics.

OPTIONS
-------

**SOURCE** [Required]
    Specifies the URI for the Robinhood backend to query.

**--output OUTPUT** [Optional]
    Defines how the results are computed and displayed. Outputs may include aggregate functions such as `sum(size)`, `min(size)`, `max(size)`, or `count()`.

**--group-by FIELD** [Optional]
    Groups the output records by the specified `FIELD` (e.g., `uid`, `type`, `hsm-state`).

**-h**, **--help**
    Shows the help message and exits.

**-v**, **--verbose**
    Displays detailed processing steps.

**--csv**
    Outputs results in CSV format.

**-d**, **--dry-run**
    Prints the generated command after alias substitution but does not execute it.

**-c**, **--config PATH**
    Specifies a custom configuration file to use.

**--alias NAME**
    Adds an alias to the operation for reuse.

USAGE EXAMPLES
--------------

1. **Basic Report of File Count**:
   ```bash
   rbh-report rbh:mongo:backend --output "count()"
   ```

2. **Group by File Type and Report Counts**:
   ```bash
   rbh-report rbh:mongo:backend --group-by "statx.type" --output "count()"
   ```

3. **Group by User and Summarize File Sizes**:
   ```bash
   rbh-report rbh:mongo:backend --group-by "user" --output "min(size),max(size),avg(size)"
   ```

4. **CSV Format Output**:
   ```bash
   rbh-report rbh:mongo:test --csv --output "sum(statx.size)"
   ```

NOTES
-----

- The available fields for `--group-by` and `--output` depend on the backend and its associated statistics.
- Outputs can be tailored for specific analyses using filters and predicates.
