rbh-stats(1)
============

SYNOPSIS
--------

**rbh-stats** *SOURCE* [**OPTIONS**]

DESCRIPTION
-----------

Print RobinHood4 tools' runs' statistics from SOURCE's metadata. These
statistics are recorded by the tools themselves.

By default, print the last run's available statistics, of any tool.

Format output modifiers only work when printing statistics, not when deleting
them or checking their count. Only one format option can be used at a time.

Mandatory arguments to long options are mandatory for short options too.

**SOURCE**
    Specify the backend to get statistics from. Should take the form of a
    RobinHood4 URI.

**-a, --after** *TIMESTAMP*
    Print statistics that were recorded after `TIMESTAMP`. `TIMESTAMP` should
    be a valid non-zero epoch.

**-b, --before** *TIMESTAMP*
    Print statistics that were recorded before `TIMESTAMP`. `TIMESTAMP` should
    be a valid non-zero epoch.

**-c, --config** *PATH*
    The configuration file to use, if not specified, will use
    `/etc/robinhood4.d/default.yaml`.

**--csv**
    Print statistics in a CSV format. Only works when printing statistics,
    and not when used with `--stats-count` or `--delete`.

**--delete**
    Delete the requested statistics instead of printing them.
    Cannot be used with `--stats-count`.

**-F, --first**
    Print the first statistics instead of the last one.

**-h, --help**
    Show the help message and exit.

**--json**
    Print statistics in a JSON format. Only works when printing statistics,
    and not when used with `--stats-count` or `--delete`.

**--stats-count**
    Print the number of tool runs currently recorded.
    Cannot be used with `--delete` and no stats will be printed.

**-n, --count** *N*
    Print `N` statistics instead of one.

**--oneline**
    Print statistics in a shortened format. Only works when printing statistics,
    and not when used with `--stats-count` or `--delete`.

**-t, --tool** *TOOLS*
    Print stats of the requested tools instead of any tool.
    `TOOLS` must be a CSV list consisting of the RobinHood4 tools 'sync',
    'find', 'report', 'gc' or 'fsevents'.

**--version**
    Print RobinHood 4's version.

EXAMPLES
--------

Get statistics about the last RobinHood4 tool run
    $ rbh-stats rbh:mongo:backend

Get statistics about the last 3 rbh-sync runs
    $ rbh-stats rbh:mongo:backend --tool sync --count 3

Delete statistics about the last 10 rbh-find, rbh-report and rbh-sync runs
    $ rbh-stats rbh:mongo:backend --tool find,report,sync --count 10

Print statistics about the last 3 runs in JSON
    $ rbh-stats rbh:mongo:backend --json --count 3

Print statistics about the first 5 rbh-fsevents and rbh-sync runs in oneline
    $ rbh-stats rbh:mongo:backend --first --tool fsevents,sync --oneline --count 5

Print statistics about the first 2 runs that happened after 1 hour ago in csv
    $ rbh-stats rbh:mongo:backend --first --count 2 --after $(( $(date +%s) - 3600 )) --csv

SEE ALSO
--------

robinhood4(5), robinhood4(7)

CREDITS
-------

rbh-stats and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
