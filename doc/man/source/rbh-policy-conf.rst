rbh-policy-conf(5)
==================

DESCRIPTION
-----------

``rbh-policy`` uses a Python configuration file to define:

- global Policy Engine settings,
- reusable fileclasses,
- policies, triggers, actions, and optional rules.

A configuration file must import the Policy Engine API and define a single
global configuration block.

BASIC STRUCTURE
---------------

.. code:: python

    from rbhpolicy.config.core import *

    config(
        filesystem="rbh:posix:/mnt/data",
        database="rbh:mongo:test",
        evaluation_interval="5s"
    )

    hot_data = declare_fileclass(
        name="hot_data",
        target=(Type == "f") & (LastAccess < "7d")
    )

    declare_policy(
        name="audit_hot_data",
        target=hot_data,
        action=action.log,
        trigger=Periodic("1d")
    )

IMPORT STYLE
------------

Recommended:

.. code:: python

    from rbhpolicy.config.core import *

Alternative (explicit namespace):

.. code:: python

    import rbhpolicy.config.core as rbh

    rbh.config(...)
    rbh.declare_policy(..., action=rbh.action.log, trigger=rbh.Periodic("1d"))

GLOBAL CONFIGURATION
--------------------

Use ``config()`` exactly once in the file.

- ``filesystem``: URI of the managed backend.
- ``database``: URI of the metadata/index backend.
- ``evaluation_interval``: evaluation period string.

Current validation checks:

- ``filesystem`` and ``database`` must be strings of the form ``rbh:X:X``.
- ``evaluation_interval`` must be a string.

Example:

.. code:: python

    config(
        filesystem="rbh:posix:/mnt/data",
        database="rbh:mongo:test",
        evaluation_interval="10s"
    )

FILTER EXPRESSIONS
------------------

Filter expressions select entries using metadata attributes.
They are used in fileclass targets, policy targets, and rule conditions.

Available Base Entities
+++++++++++++++++++++++

Generic entities currently translated by the filter DSL are:

- ``Path``: full path matching (glob-style, case-sensitive).
- ``Name``: basename matching (glob-style, case-sensitive).
- ``IName``: basename matching (glob-style, case-insensitive).
- ``Type``: entry type.
- ``Size``: file size comparisons.
- ``User``: owner user name.
- ``UID``: owner user ID.
- ``Group``: owner group name.
- ``GID``: owner group ID.
- ``LastAccess``: time since last access.
- ``LastModification``: time since last content modification.
- ``LastChange``: time since last metadata/status change.
- ``CreationDate``: file creation/birth time.
- ``DirCount``: directory entry count.

Notes:

- Additional entities can exist depending on backend/extensions.
- If an entity is not supported by the current translation layer,
  configuration loading fails with an explicit error.

Operators and Composition
+++++++++++++++++++++++++

Comparison operators on entities:

- ==
- !=
- <
- <=
- >
- >=

Logical composition:

- ``&`` for AND
- ``|`` for OR
- ``~`` for NOT

Use parentheses around each condition in composite expressions to improve
visibility.
Examples:

.. code:: python

    (Size >= "1GB") & (LastAccess > "180d")
    (User == "admin") | (User == "root")
    ~(Type == "d")

Accepted Value Formats
++++++++++++++++++++++

Type
^^^^

Accepted values include:

- file, f
- dir, d
- symlink, l
- block, b
- char, c
- fifo, p
- socket, s

Size
^^^^

Accepted size units include B, KB, MB, GB, TB, PB, EB
(and short forms c, k, M, G, T, P, E).

Examples:

- 10MB
- 1GB
- 42c

Time-based entities
^^^^^^^^^^^^^^^^^^^

For LastAccess, LastModification, LastChange, CreationDate,
accepted relative durations include:

- m, h, d, w, mo, y
- minute(s), hour(s), day(s), week(s), month(s), year(s)

Examples:

- 30m
- 2h
- 7d
- 3weeks
- 6months

Absolute date strings are also parsed (examples):

- 2026-03-17
- 2026-03-17T13:45:00
- 17/03/2026
- 2026/03/17

UID, GID, DirCount
^^^^^^^^^^^^^^^^^^

- UID and GID accept numeric values.
- DirCount accepts numeric/quantity-like values.

FILECLASSES
-----------

A fileclass defines a reusable target set.

.. code:: python

    cold_large = declare_fileclass(
        name="cold_large",
        target=(Type == "f") & (Size >= "10GB") & (LastAccess > "180d")
    )

Rules:

- ``name`` must be a string.
- ``target`` must be a valid logical condition.

POLICIES
--------

A policy is declared with ``declare_policy()``.

.. code:: python

    declare_policy(
        name="cleanup",
        target=(Type == "f") & (LastAccess > "180d"),
        action=action.delete,
        trigger=Periodic("1d"),
        parameters={"remove_empty_parent": True}
    )

Validation rules:

- ``name`` must be a string.
- ``target`` must be a logical condition.
- ``action`` must be a supported action value (string/callable/``None``).
- ``trigger`` should be a trigger object or ``None``.
- ``parameters`` should be a ``dict``.
- ``rules`` must be a ``Rule`` or a list/tuple of ``Rule``.

ACTIONS
-------

Built-in Actions
++++++++++++++++

Built-ins are exposed by the ``action`` namespace:

- ``action.log``
- ``action.delete``

Short string forms also work:

- ``"log"``
- ``"delete"``

Extension-scoped built-ins are available to force a specific implementation:

- ``posix.log``, ``posix.delete``
- ``lustre.log``, ``lustre.delete``
- ``retention.log``, ``retention.delete``

Why use scoped forms:

- ``action.log`` or ``"log"`` selects the common built-in path.
- ``posix.log`` or ``lustre.log`` explicitly requests that extension namespace.

Examples:

.. code:: python

    action=action.log
    action=posix.log
    action=lustre.delete

Delete action behavior
++++++++++++++++++++++

``delete`` removes matching filesystem entries.

Current behavior to keep in mind:

- files are removed,
- empty directories can be removed,
- non-empty directory deletion does not remove content recursively.

Supported ``delete`` parameters:

- ``remove_empty_parent`` (``bool``, default ``False``)
  If True, after deleting an entry, the engine tries to remove now-empty
  parent directories upward.

- ``remove_parents_below`` (string path)
  Defines an upper floor for parent cleanup. Parent removal stops at this
  directory (it is preserved).

Important:

- ``remove_parents_below`` has effect only when ``remove_empty_parent`` is ``True``.

Example:

.. code:: python

    declare_policy(
        name="delete_old_files",
        target=(Type == "f") & (LastAccess > "365d"),
        action=action.delete,
        parameters={
            "remove_empty_parent": True,
            "remove_parents_below": "archive"
        },
        trigger=Periodic("7d")
    )

Log action behavior
+++++++++++++++++++

``log`` prints matched entries for auditing/debugging.

Typical output includes:

- ``path=...``
- ``params=...``

On Lustre contexts, a ``fid`` field is also printed.

Shell Command Actions (cmd)
+++++++++++++++++++++++++++

Use ``cmd()`` to execute a shell command on each matched entry.

.. code:: python

    action=cmd("archive_tool --level {lvl} {}")
    parameters={"lvl": 3}

Placeholder rules:

- ``{}`` is reserved for the matched entry path.
- ``{name}`` placeholders are substituted from ``parameters``.

Validation behavior:

- unresolved named placeholders raise a configuration error,
- ``cmd()`` expects a string argument.

Runtime behavior:

- command execution failures are reported,
- policy execution continues on other entries.

Custom Python Actions
+++++++++++++++++++++

You can pass a Python callable as ``action``.

Expected callable style:

- ``def my_action(path, **kwargs): ...``

Practical recommendations:

- always accept ``path`` as first argument,
- use keyword parameters (or ``**kwargs``) to receive values from ``parameters``,
- return 0 on success.

Constraints:

- lambda actions are rejected,
- function can be local to config file or imported from another module.

PARAMETERS
----------

``parameters`` is a dictionary associated with either a policy or a rule.

Main uses:

- built-in action tuning (for example ``delete`` options),
- ``cmd()`` placeholder substitution,
- keyword arguments for Python callables.

Examples:

.. code:: python

    parameters={
        "remove_empty_parent": True,
        "lvl": 3,
        "tag": "archive"
    }

Rule-level ``parameters`` are local to the rule action.

RULES
-----

``Rule`` objects refine policy behavior for subsets of target entries.
Rules are evaluated in order, and first matching rule wins.

Each ``Rule`` contains:

- ``name`` (string, required)
- ``condition`` (logical condition, required)
- ``action`` (use ``None`` to ignore)
- ``parameters`` (optional ``dict``)

Example:

.. code:: python

    declare_policy(
        name="mixed_policy",
        target=(Type == "f") | (Type == "d"),
        action=action.log,
        trigger=Periodic("1d"),
        rules=[
            Rule(
                name="delete_only_files",
                condition=(Type == "f"),
                action=action.delete,
                parameters={"remove_empty_parent": False}
            ),
            Rule(
                name="skip_root_owned",
                condition=(User == "root"),
                action=None
            )
        ]
    )

TRIGGERS
--------

A policy ``trigger`` controls whether a policy is eligible for execution.

Time-based trigger constructors:

- ``Periodic("10m")``, ``Periodic("1h")``, ``Periodic("1d")``
- ``Scheduled("2026-03-17 03:00")``
- ``Scheduled("2026-03-17 03:00:00")``

Capacity/database trigger constructors:

- ``GlobalSizePercent(80)``
- ``GlobalInodePercent(90)``
- ``UserFileCount("alice,bob", 100000)``
- ``UserDiskUsage("alice", 500 * 1024**3)``
- ``UserInodeCount("alice", 1000000)``
- ``GroupFileCount("staff", 200000)``
- ``GroupSize("staff", 1024**4)``
- ``GroupInodeCount("staff", 200000)``

Compositions:

- ``trigger_a & trigger_b``
- ``trigger_a | trigger_b``

Manual mode note:

- ``rbh-policy run`` evaluates policies with ``manual_mode`` enabled.
- In manual mode, ``Periodic()`` and ``Scheduled()`` do not block execution.

API ELEMENTS (DO NOT REDEFINE)
------------------------------

When using ``from rbhpolicy.config.core import *``, do not redefine imported API
names such as:

- ``config``
- ``declare_policy``
- ``Rule``
- ``declare_fileclass``
- ``cmd``
- ``action``, ``posix``, ``lustre``, ``retention``
- ``Periodic``, ``Scheduled``, trigger helper constructors
- filter entities such as ``Path``, ``Name``, ``Type``, ``Size``, ``User``

Redefining these names can silently shadow API objects and break behavior.

SPLITTING CONFIGURATION ACROSS MULTIPLE FILES
---------------------------------------------

Since a configuration file is a standard Python module, you can use regular
Python imports to spread a large configuration across several files.

This is useful to:

- share common fileclasses between several configurations,
- separate fileclasses, policies, and triggers into dedicated modules,

Shared fileclass module
+++++++++++++++++++++++

In a shared module (for example ``common_fileclasses.py``):

.. code:: python

    from rbhpolicy.config.core import *

    cold_large = declare_fileclass(
        name="cold_large",
        target=(Type == "f") & (Size >= "10GB") & (LastAccess > "180d")
    )

    hot_small = declare_fileclass(
        name="hot_small",
        target=(Type == "f") & (Size < "1MB") & (LastAccess < "1d")
    )

Then in the main configuration file:

.. code:: python

    from rbhpolicy.config.core import *
    from common_fileclasses import cold_large, hot_small

    config(
        filesystem="rbh:posix:/mnt/data",
        database="rbh:mongo:test",
        evaluation_interval="10s"
    )

    declare_policy(
        name="archive_cold",
        target=cold_large,
        action=action.log,
        trigger=Periodic("1d")
    )

Separated policies module
+++++++++++++++++++++++++

Policies can be defined in a separate module and imported as a group.
Because ``declare_policy()`` registers policies globally at import time,
simply importing the module is sufficient to register all its policies:

.. code:: python

    # policies/archive.py
    from rbhpolicy.config.core import *
    from common_fileclasses import cold_large

    declare_policy(
        name="archive_cold",
        target=cold_large,
        action=action.delete,
        trigger=Periodic("7d"),
        parameters={"remove_empty_parent": True}
    )

.. code:: python

    # main config file
    from rbhpolicy.config.core import *
    import policies.archive  # registers all policies defined in that module

    config(
        filesystem="rbh:posix:/mnt/data",
        database="rbh:mongo:test",
        evaluation_interval="10s"
    )

SEE ALSO
--------

rbh-policy(1), robinhood4(7)

CREDITS
-------

rbh-policy and RobinHood4 is written by the storage software development team
of the Commissariat a l'energie atomique et aux energies alternatives.

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
