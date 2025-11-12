rbh-find
========

NAME
====

rbh-find - query and filter metadata from one backend

SYNOPSIS
========

rbh-find [PRE-OPTIONS]... SOURCE [PREDICATE]... [ACTION]...
rbh-find [ALIAS]

DESCRIPTION
===========

Query SOURCE's entries according to a list of predicates and execute an action
on each. The predicates available depend on the backends that were used to
update metadata in SOURCE (this can be known using rbh-info(1)).

SOURCE must take the form of an URI, and it must correspond to a backend that
can be filtered (this can be known using rbh-info(1)).

rbh-find is a close twin of GNU's find(1), so here are only listed the
differences between the two.

Mandatory arguments to long options are mandatory for short options too.

PRE-OPTIONS
===========

-c, --config=PATH
                  the configuration file to use, if not specified, will use
                  `/etc/robinhood4.d/default.yaml`

-d, --dry-run     displays the command after alias replacing every alias in it,
                  and stop the command
-h, --help[=BACKEND]
                  displays the command's helper. If a BACKEND is provided, also
                  show the BACKEND's helper, which includes the list of
                  available predicates for that backend
-v, --verbose     show the request sent to SOURCE after all predicates have
                  been converted to SOURCE's filtering system (e.g. the MongoDB
                  bson request sent to the server)

ALIAS
=====

--alias=ALIAS     specify an alias for the operation. Aliases are a way to
                  shorten the command line by specifying frequently used
                  components in the configuration file under an alias name, and
                  using that alias name in the command line instead.

PREDICATE
=========

The predicates are not tool specific but instead are per backend, so use the
following to know the available predicates:
    $ rbh-find --help [BACKEND]

For instance:
    $ rbh-find --help posix
    $ rbh-find --help lustre

ACTION
======

The given action will be executed on each entry matched by the predicates
specified earlier in the command line.

-count            count the number of entries
-[r]sort FIELD    sort or reverse sort entries based on the FIELD requested

Following is a list of available actions that are replicas of GNU's find's
actions:
    -delete
    -exec
    -print
    -print0
    -printf
    -fprint
    -fprintf
    -ls

Similarly to predicates, directives for the `-[f]printf` action are on a
per-backend basis, so use the following to know the available directives:
    $ rbh-find --help [BACKEND]

For instance:
    $ rbh-find --help posix
    $ rbh-find --help lustre

EXAMPLES
========

Find all entries in a Mongo database with a size larger than 3GB:
    $ rbh-find rbh:mongo:blob -size +3G

Find all files in an SQLite database and output their path and size:
    $ rbh-find rbh:sqlite:blob.sqlite -type f -printf "%p: %s\n"

Find all entries in a Mongo database belonging to uid 3000 and delete them:
    $ rbh-find rbh:mongo:blob -uid 3000 -delete

SEE ALSO
========

find(1), rbh-info(1), rbh4(5), rbh4(7)

CREDITS
=======

rbh-sync and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
======

rbh-find and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
