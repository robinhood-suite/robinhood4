rbh-report(1)
=============

NAME
----

rbh-report - Generate aggregated reports from Robinhood backends

SYNOPSIS
--------

**rbh-report** [**OPTIONS**] *SOURCE* *--output OUTPUT* [**--group-by FIELD**] [*PREDICATES*]

DESCRIPTION
-----------

Query SOURCE's entries and generate reports based on aggregated information. By
default, all entries are grouped into one, and `rbh-report` uses OUTPUT to know
how to aggregate the information.

Entries can also be grouped in other ways, by using the **--group-by** option.
Moreover, entries can be filtered beforehand by specifying predicates in a
similar fashion to rbh-find(1).

ARGUMENTS
---------

PARAMETERS
++++++++++

**SOURCE**
    Specify the backend to get aggregated information from. Should take the
    form of a RobinHood4 URI.

**--output** *OUTPUT*
    Define how the results are computed and displayed. Output must be given
    as a string in CSV format, with each value an aggregate function applied
    on a statx value. The aggregate functions include: `sum`, `min`, `max`,
    `avg` and `count` (the latter doesn't need any statx value to work).

OPTIONS
+++++++

**--group-by** *FIELD*
    Group the output records by the specified `FIELD` (e.g., `uid`, `type`,
    `hsm-state`). Must be a string in CSV format, with each value a statx value
    to group entries on.

**-h, --help**
    Show the help message and exits.

**-v, --verbose**
    Display the request sent by the backend to the underlying storage system.

**--csv**
    Output results in CSV format.

**-d, --dry-run**
    Print the generated command after alias substitution but does not execute
    it.

**-c, --config** *PATH*
    Specify the configuration file to use.

**--alias** NAME
    Specify an alias for the operation. Aliases are a way to shorten the
    command line by specifying frequently used components in the configuration
    file under an alias name, and using that alias name in the command line
    instead.

EXAMPLES
--------

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
   rbh-report rbh:mongo:backend --group-by "statx.user" --output "min(statx.size),max(statx.size),avg(statx.size)"
   ```

4. **CSV Format Output**:
   ```bash
   rbh-report rbh:mongo:test --csv --group-by "statx.user,statx.type" --output "sum(statx.size),count()"
   ```

SEE ALSO
--------

rbh-find(1), rbh4(5), rbh4(7)

CREDITS
-------

rbh-report and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
------

rbh-report and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
