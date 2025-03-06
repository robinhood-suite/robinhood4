.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
                      alternatives

   SPDX-License-Identifer: LGPL-3.0-or-later

#################
RobinHood filters
#################

This design document describes the filtering mechanism of the RobinHood suite.
It presents what they are used for and how they are currently implemented in
``rbh-find`` tools. As this mechanism will evolve and be a part of new
``rbh-find`` overloads or with other RobinHood tools like ``rbh-report``, it
becomes interesting to mutualise filters between all RobinHood tools and ease
the way they can be used and enriched.

Disclaimer: this ADR is partly based on the ADR "enrichers_and_iterators.rst"

Current implementation
======================

It currently exists three different tools which deal with filters:

* ``rbh-find``, for actions and POSIX filters;
* ``rbh-lfind``, extending the first one with Lustre filters;
* ``rbh-ifind``, extending the first one with Hestia filters.

The following sections will describe some important details of their
implementation.

Tokens
------

Tokens are options given through the command line. It exists three different
kind of tokens: predicates, actions and reserved tokens. Predicates are options
that define a filter. Actions are options that ask for a task to be executed
on the retrieved information. Reserved tokens stand for tokens that are used to
build the backend request, which non-exhaustively includes ``(``, ``)``, ``;``,
``-and`` and ``-sort``.

For instance, the following command will see the options according to the
table ``Tokens``:

.. code-block:: bash

  $ rbh-find rbh:mongo:blob -size +3G -and -not -type d -sort size -ls

.. list-table:: Tokens
  :widths: 25 25 25
  :header-rows: 1

  * - Predicates
    - Actions
    - Reserved tokens
  * - ``-size``
    - ``-ls``
    - ``-and``
  * - ``-type``
    -
    - ``-not``
  * -
    -
    - ``-sort``

Parser & filter generation
--------------------------

The parser part consists in converting tokens given by the user as
command line options to ``rbh_filter``. To that extent, this process is divided
into three steps:

* determine if the token is a predicate, an action or a reserved token;
* determine if the token is valid;
* generate the associated filter.

Three different parsers exist: the POSIX, the Lustre and the Hestia ones,
latters being extensions of the foremost. The extension consists in calling
the POSIX parser if the token is not recognised by the other parsers.

Action
------

Each recognised action will lead to a callback which operates on entries
retrieved from a backend. Two tools implement actions for now: ``rbh_find``
and ``rbh_report``. Their actions are different so we do not need to mutualise
them.

Issues with the current implementation
--------------------------------------

Until now, there are major issues with filters in RobinHood:

#. filters are implemented among ``rbh-find`` and need to be exported to other
   tools;
#. filters depend on plugins, which define the entries data structure;
#. if multiple overloads are possible, all combinations need to be implemented,
   for instance POSIX with Lustre, POSIX with SELinux, POSIX with SELinux and
   Lustre.

Possible solutions
==================

Several solutions can be implemented to ensure filters are shared through the
whole RobinHood 4 suite.

Just mutualising filters
------------------------

Considering tools overload is not an issue, we can mutualise the parser and the
filter generation to ``librobinhood``.

Pros: minimal effort for the implementation, mainly consists in moving code
from ``rbh-find`` to ``librobinhood``.
Cons: still need to have different tools for each set of filters i.e.
``rbh-find`` and ``rbh-report`` for POSIX filters, and ``rbh-lfind`` and
``rbh-lreport`` for Lustre filters.

Selecting filters using an option
---------------------------------

Overloading the tools can be shortcut by adding an option to filter tools that
will load the desired parsers and filters. This option can be hidden using a
command alias or a configuration alias.

Pros: avoid having multiple tools for each set of filters.
Cons: still need for the user to indicate which set of filters is requested.

Adding synchronised backend to the database
-------------------------------------------

Usually, and because filters are plugin-dependent, users want to have available
filters corresponding to the information stored in the database from a
synchronisation. In this case, it is interesting to store in the database which
backend and plugins were used during the synchronisation. Thus, the tool will
automatically load the correct filters by asking the targeted database.

Pros: avoid having the user indicating which set of filters is requested.
Cons: move the specific parser code to the plugin definition; is this really
an issue?

Adding it all to the database
-----------------------------

Because why not, each synchronisation process can store in the database a
dictionary containing for each filter:

* its command line name;
* its associated field in the entry;
* its way to generate the filter.

Thus, when a command needs to parse some filters, this dictionary is retrieved
from the database pointed by the URI, and the parsing can be executed.

Pros:

* no need to have multiple libraries or plugin calls to manage the parsing and
  the filter generation;
* all the plugin-dependent code is located in its backend.

Cons:

* code directly added to the database, may lead to security issues;
* code interpretation may be difficult to implement;
* need to have database migration scripts in case the filter generation change.

PS1: Mongo can execute code stored in an entry, if written in JavaScript, but
that may not be the case for each mirror backend.

PS2: For a lot of fun, one can still add all codes written in C as strings in
the database, retrieve them at the beginning of an ``rbh-find`` command,
aggregate and compile them in a C library, and finally open it dynamically..

Steps to refactoring
====================

Finally, the chosen solution tends to be the third one, having some metadata
added to the mirror database which tell the original backend and its enrichers.
This leads to the following process:

#. ``rbh-sync:mirror``: store used backend and enricher names
#. ``rbh-find:mirror``: fetch backend and enricher names
#. ``rbh-find:filesys``: parse command line
#. ``rbh-find:mirror``: execute filter request

Exporting the filters
---------------------

As filters will be needed in other tools, we need to offer them in
``librobinhood``, and thus avoiding having dependencies between RobinHood tools.

To this extent, the code just need to be moved.

Multiple parsers
----------------

Each backend or enricher define its own entry fields and filters. Thus it is
mandatory to have one parser per plugin.

That may lead to option name collision, in case a backend and several of its
enrichers want to define a ``-size`` filter for instance. In this case, we can
stick to the order of the enrichers defined in the configuration file, and
thus stored in the database: the last one in the chosen one.

For instance, with the following backend:

.. code-block:: yaml

   backends:
       blob:
           extends: posix
           enrichers:
               - betterposix
               - bettersize

Considering the original backend and both enrichers define the ``-size`` filter,
the one used during the filter generation will be the ``bettersize`` one.

Plugin integration
-------------------

One way to add the filter generation, is to define a new operation for the
plugin:

.. code-block:: C

   struct rbh_plugin_operations {
       ...
       struct rbh_filter *(*parse_predicate)(
           void *plugin,
           char **argv,
           int argc,     // number of tokens in argv
           int *arg_idx  // current token index
       );
   };

Then, when parsing the command line arguments in `rbh-find` or `rbh-report`, if
a predicate is encountered we call those callbacks in plugins.

For now, both the check and the filter generation are done in the same command.
But we prefer having those behaviors in two different calls, like the following:

.. code-block:: C

   if (!rbh_plugin_check_valid_token(lustre_plugin, argv[i]))
       goto try_next_plugin;

   filter = rbh_plugin_build_filter(lustre_plugin, argv, argc, &i);

That will not deeply change the behavior of the feature, but will ease the
understanding of the code. Also, if actions are plugin-specific, it is
mandatory to split the check and the filter:

.. code-block:: C

   token_kind = rbh_plugin_check_valid_token(lustre_plugin, argv[i]));
   if (token_kind == RBH_TOKEN_UNKNOWN)
       goto try_next_plugin;

   if (token_kind == RBH_TOKEN_PREDICATE)
       filter = rbh_plugin_build_filter(lustre_plugin, argv, argc, &i);
   else
       rbh_plugin_find(lustre_plugin, filter, argv, argc, &i);

What about actions?
-------------------

As the ``-printf`` action has different behaviors with POSIX and Lustre
backends, we will need to implement a plugin operation for specific actions.

Actions that are not plugin-specific do not need to be moved to
``librobinhood``.

There may be some actions which are tool-specific, like the ``-printf`` one.
This imply that the check function of the plugin need to consider which tool
is involved, to validate the token.

What about reserved tokens?
-------------------------

Reserved tokens will not be plugin-specific, but are needed for all filter
parsing. To avoid code duplication, it is wise to move it to ``librobinhood``.
