rbh-report(1)
=============

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

Mandatory arguments to long options are mandatory for short options too.

**SOURCE**
    Specify the backend to get aggregated information from. Should take the
    form of a RobinHood4 URI.

**--output** *OUTPUT*
    Define how the results are computed and displayed. Output must be given
    as a string in CSV format, with each value an aggregate function applied
    on a statx value. The aggregate functions include: `sum`, `min`, `max`,
    `avg` and `count` (the latter doesn't need any statx value to work).

**--alias** NAME
    Specify an alias for the operation. Aliases are a way to shorten the
    command line by specifying frequently used components in the configuration
    file under an alias name, and using that alias name in the command line
    instead.

**-c, --config** *PATH*
    Specify the configuration file to use.

**--csv**
    Output results in CSV format.

**-d, --dry-run**
    Print the generated command after alias substitution but does not execute
    it.

**--group-by** *FIELD*
    Group the output records by the specified `FIELD` (e.g., `uid`, `type`,
    `hsm-state`). Must be a string in CSV format, with each value a statx value
    to group entries on.

**-h, --help**
    Show the help message and exits.

**-v, --verbose**
    Display the request sent by the backend to the underlying storage system.

EXAMPLES
--------

Basic Report of File Count
    $ rbh-report rbh:mongo:backend --output "count()"

Group by File Type and Report Counts
    $ rbh-report rbh:mongo:backend --group-by "statx.type" --output "count()"

Group by User and Summarize File Sizes
    $ rbh-report rbh:mongo:backend --group-by "statx.user" --output "min(statx.size),max(statx.size),avg(statx.size)"

CSV Format Output
    $ rbh-report rbh:mongo:test --csv --group-by "statx.user,statx.type" --output "sum(statx.size),count()"

SEE ALSO
--------

rbh-find(1), robinhood4(5), robinhood4(7)

CREDITS
-------

rbh-report and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
