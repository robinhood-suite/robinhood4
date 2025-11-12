rbh-find(1)
===========

NAME
----

rbh-find - query and filter metadata from one backend

SYNOPSIS
--------

**rbh-find** [**OPTIONS**] *SOURCE* [*SOURCES*] [*PREDICATE*] [*ACTION*]

DESCRIPTION
-----------

Query SOURCE's entries according to a list of predicates and execute an action
on each. The predicates available depend on the backends that were used to
update metadata in SOURCE (this can be known using rbh-info(1)).

At least one SOURCE must be specified, but more can be given.

rbh-find is a close twin of GNU's find(1), so here are only listed the
differences between the two.

Mandatory arguments to long options are mandatory for short options too.

ARGUMENTS
---------

PARAMETERS
++++++++++

**SOURCE**
    Specify the backend to filter on and get entries from. Should take the
    form of a RobinHood4 URI.

PRE-OPTIONS
+++++++++++

**-c, --config** *PATH*
    The configuration file to use, if not specified, will use
    `/etc/robinhood4.d/default.yaml`.

**-d, --dry-run**
    Display the command after alias replacing every alias in it, and stop the
    command.

**-h, --help** *[=BACKEND]*
    Display the help message. If a BACKEND is provided, also show the BACKEND's
    helper, which includes the list of available predicates for that backend.

**-v, --verbose**
    Show the request sent to SOURCE after all predicates have been converted to
    SOURCE's filtering system (e.g. the MongoDB bson request sent to the
    server).

PREDICATES AND ACTIONS
++++++++++++++++++++++

The predicates are not tool specific but instead are per backend, so use the
following to know the available predicates:
    $ rbh-find --help [BACKEND]

For instance:
    $ rbh-find --help posix
    $ rbh-find --help lustre

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
per-backend basis, so use `rbh-find`'s helper to know the available directives.

GNU-Find actions VS rbh-find actions
------------------------------------

gnu-find can be compared to a configurable sorting machine.

For example, when running the following command:

.. code:: bash

    find -type f -name '*.txt' -print

The first thing find does is build a tree -- or rather, a pipeline -- of its
command line's predicates (``-type f``, ``-name '*.txt'``) and actions
(``-print``)::

                                           true  --------- (always) true  -----
                                              -->| print |--------------->| ø |
                   true  -------------------  |  ---------                -----
                      -->| name =~ ".txt$" |--|
    ----------------  |  -------------------  |  -----
    | type == FILE |--|                       -->| ø |
    ----------------  |  -----            false  -----
                      -->| ø |
                  false  -----

Then it traverses the current directory (because "." is implied), and its
subdirectories, and their subdirectories, ... And each filesystem entry it
encounters goes through the pipeline. Once.

Now, find allows you to place multiple actions on the command line:

.. code:: bash

    find -print -print

This is also converted into a single tree::

    --------- (always) true  --------- (always) true  -----
    | print |--------------->| print |--------------->| ø |
    ---------                ---------                -----

And each entry is still only processed once (it is printed twice, but iterated
on once).

rbh-find works a little differently. Since it uses RobinHood backends, it can
query all the entries that match a set of predicates at once, rather than
traverse a tree of directories looking for them. But it cannot ask the backend
to run actions on those entries: it has to perform them itself.

The execution flow looks like this::

    ---------   ----------
    | query |-->| action |
    ---------   ----------

And when there are multiple actions::

    -----------   ------------   -----------   ------------
    | query-0 |-->| action-0 |-->| query-1 |-->| action-1 |
    -----------   ------------   -----------   ------------

Where ``query-1`` is a combination of ``query-0`` and whatever predicates appear
between ``action-0`` and ``action-1``.

Another approach would be to fall back to a regular find pipeline after
``action-0``. But this would require reimplementing all the filtering logic of
find, and there is no garantee that it would be faster than issuing a new query.
So rbh-find does not do it that way.

But what are the consequences of such a choice?

There are three:

#. for every action, rbh-find sends one query per URI on the command line;
#. rbh-find's output is not ordered the same way find's is;
#. rbh-find's actions do not filter out any entries.

An example of the difference in the output ordering:

.. code:: bash

    find -print -print
    ./a
    ./a
    ./a/b
    ./a/b
    ./a/b/c
    ./a/b/c

    rbh-find rbh:mongo:test -print -print
    ./a
    ./a/b
    ./a/b/c
    ./a
    ./a/b
    ./a/b/c


The third difference is probably the most problematic. In all the previous
examples, we used the action ``-print`` which always evaluates to ``true`` and
so does not filter out any entries. But there are other actions that do exactly
that:

.. code:: bash

    # find every file that contains 'string'
    find -type f -exec grep -q 'string' {} \; -print

The same query, ran with rbh-find would simply print each file and directory
under the current directory. Implementing the same behaviour as find is not
impossible: it would simply require keeping track of entries that "failed"
actions and exclude them from the next queries. But remembering those entries
could prove prohibitively expensive in terms of memory consumption. Moreover the
time to build the queries would increase as we exclude more and more entries.

EXAMPLES
--------

1. **Find all entries in a Mongo database with a size larger than 3GB**:
   ```bash
   rbh-find rbh:mongo:blob -size +3G
   ```

2. **Find all files in an SQLite database and output their path and size**:
   ```bash
   rbh-find rbh:sqlite:blob.sqlite -type f -printf "%p: %s\n"
   ```

3. **Find all entries in a Mongo database belonging to uid 3000 and delete
   them**:
   ```bash
   rbh-find rbh:mongo:blob -uid 3000 -delete
   ```

SEE ALSO
--------

find(1), rbh-info(1), rbh4(5), rbh4(7)

CREDITS
-------

rbh-find and RobinHood4 are distributed under the GNU Lesser General Public
License v3.0 or later. See the file COPYING for details.

AUTHOR
------

rbh-find and RobinHood4 is written by the storage software development team of
the Commissariat à l'énergie atomique et aux énergies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
