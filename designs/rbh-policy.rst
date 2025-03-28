.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

------------------------------------------------------------
RobinHood 4 Policy Engine – Design Document
------------------------------------------------------------

Overview
========
The RobinHood 4 Policy Engine is designed to automate the execution of specific
actions on files based on rules defined by administrators.
This document describes the functionality, configuration syntax, and high-level
implementation of the policy engine.
The engine is executed via the ``rbh-policy`` command, which takes the
configuration file and the name of the policy to execute as arguments.
A daemon will be added later to manage the execution of policies automatically.

Goals
=====
* Provide a simple and clear configuration interface for administrators.
* Enable file grouping via reusable file classes based on criteria.
* Allow natural expression of file filtering conditions using Python’s logical
  operators.
* Automate the execution of actions on files based on administrator-defined
  rules.
* Allow policies to be executed via the ``rbh-policy`` command,
  which accepts the configuration file and the policy name as arguments.
* Allow administrators to incorporate advanced Python behaviors in their
  configuration for customized functionality.
* Leverage the underlying RobinHood 4 C API to perform efficient file filtering
  and action execution.

Configuration Syntax
====================

RobinHood 4 Policy Engine uses a structured configuration format that allows
administrators to define:
- Fileclasses: reusable conditions to match specific sets of files.
- Policies: defined set of rules consisting of its name, target,
  the action to be executed, and the trigger that activates it.

The configuration consists of two main declarations:
1. Fileclasses Declaration: ``declare_fileclass``
2. Policy Declaration: ``declare_policy``

Fileclasses Declaration
----------------------
A fileclass defines a set of files based on a combination of filters.
These file classes can be reused across multiple policies to avoid redundancy.

**Syntax:**

.. code:: python

    declare_fileclass(
        name = "<class_name>",
        condition = <filter_condition>
    )

**Example:**

Define a file class for large, old files:

.. code:: python

    declare_fileclass(
        name = "exemple_fileclass",
        condition = (Size >= "10G") & (Last_Access > "180d")
    )

This file class matches all files that are larger than 10GB and not accessed in
the last 180 days.



###REVOIR CECI EST UN DRAFT

Policy Declaration
------------------

A policy defines a set of rules that determine how RobinHood should manage
specific files.
Each policy consists of a mandatory configuration and optional customization
blocks.

**Syntax:**

.. code:: python

    declare_policy(
        name = "<policy_name>",
        target = <fileclass_condition | expression>,
        action = <function_reference>,
        trigger = <optional_trigger_condition>,
        parameters = {
            <optional_action_parameters>
        },
        rules = [
            {
                "name": "<rule_name>",
                "target": <fileclass_condition | expression>,
                "action": <optional_override_action>,
                "parameters": {
                    <optional_override_parameters>
                }
            },
        ]
    )

**Components of a Policy**

1. ``name`` (mandatory):
   - A unique string identifier for the policy. This name is used to reference
     and manage the policy within the configuration.
   - Example: ``archive_large_files``

2. ``target`` (mandatory):
   - Defines the set of files to which the policy applies. This can be:
     - A reference to a single fileclass, e.g., ``recent_files``.
     - A combination of fileclasses using logical operators:
       - Intersection: ``recent_files & large_files``
       - Union: ``temp_files | backup_files``
       - Negation: ``~excluded_files``
     - A condition based on file attributes, e.g.,
       ``(Size >= 10) & (Last_Access > 180)``.
     - You can also combine file attributes with fileclass references, e.g.,
       ``(Size >= 10) & (Last_Access > 180) & recent_files``.

3. ``action`` (mandatory):
    - Specifies the function or command to execute when the policy applies.
    - Can be a predefined function from the RobinHood API.
    - Or a custom Python function defined by the administrator.
    - Can also be an external command or shell command.
    - Example:
      - ``action = archive_files`` (predefined function to archive files)
      - ``action = custom_action_function`` (custom Python function)
      - ``action = cmd("archive_tool --path {fullpath}")`` (external command)

4. ``parameters`` (optional):
   - A dictionary of key-value pairs defining additional parameters for the
     action.
   - Example:
     - ``parameters = {"compression": "gzip", "nb_threads": 5}``

5. ``trigger`` (optional):
   - Defines a condition under which the policy is automatically executed.
     - Common triggers include:
       - ``{ "PoolUsage": ["data_pool1", "data_pool2"], "Threshold": ">80%" }``
         (Trigger when the specified pools exceed 80% usage)
       - ``{ "OstUsage": ["ost_0", "ost_1"], "Threshold": ">85%" }``
         (Trigger when specified OSTs exceed 85% usage)
       - ``{ "UserUsage": ["user42", "user99"], "Threshold": ">1M files" }``
         (Trigger when specified users exceed 1 million files)
       - ``{ "GroupUsage": ["groupA", "groupB"], "Threshold": ">5T" }``
         (Trigger when specified groups exceed 5 terabytes of storage)
       - ``{ "GlobalUsage": ">90%" }``
         (Trigger when overall filesystem usage exceeds 90%)
       - ``{ "Periodic": "daily" }``
         (Trigger automatically on a daily schedule)
       - ``{ "Scheduled": "2024-06-01 03:00" }``
         (Run the policy at a specific date and time)
     - Note: More complex triggers may be implemented in the future. Feel free
       to suggest additional trigger conditions if needed.

6. ``rules`` (optional):
   - A list of rules that apply to subsets of the policy's target. Each rule can
     override specific details of the policy’s action, such as the target files,
     the action to be executed, and any action parameters.
   - Each rule includes:
     - ``name``: A unique string identifier for the rule
       (e.g., ``"archive_recently_modified"``).
     - ``target``: Defines a subset of the main policy target. This can be a
       specific fileclass or condition that further refines the files this rule
       applies to.
     - ``action`` (optional): Overrides the default action for this rule,
       allowing different actions for different conditions.
     - ``parameters`` (optional): Overrides the action parameters for this
       specific rule.

**Example: Migration of the "cleanup" policy from RBH3 to RBH4**

.. code:: python

    declare_policy(
        name = "cleanup",
        target = (Type == "file"),
        action = cmd("/usr/sbin/rbh_cleanup_trash.sh /ccc/fsname/scratch {path}"),
        parameters = {
            "nb_threads": 5,
            "suspend_error_pct": "50%",
            "suspend_error_min": 1000,
            "schedulers": "common.rate_limit",
            "rate_limit": {
                "max_count": 50,
                "period_ms": 1000
            }
        },
        trigger = { "Periodic": "10m" },
        rules = [
            {
                "name": "ignore_root_and_nfsnobody",
                "target": Owner == "root" | Owner == "nfsnobody",
                "action": None
            },
            {
                "name": "ignore_work_fileclass",
                "target": work,
                "action": None
            },
            {
                "name": "ignore_somegroup_fileclass",
                "target": somegroup,
                "action": None
            },
            {
                "name": "default_cleanup",
                "target": (LastAccess > 60),
                "action": None
            }
        ]
    )

This policy automates file cleanup in the scratch filesystem by:
- Targeting all files.
- Executing the cleanup script ``/usr/sbin/rbh_cleanup_trash.sh`` with the
  ``{path}`` placeholder.
- Configuring parameters such as thread count, error suspension, and rate limiting.
- Automatically triggering every 10 minutes.
- Ignoring files owned by ``root`` or ``nfsnobody``, as well as files matching
  ``work`` or ``somegroup`` fileclasses.
- Cleaning up files older than 60 days based on last access and creation time.

Policy Condition Filters
========================
The RobinHood 4 Policy Engine allows administrators to define file selection
criteria using specific filters in the policy configuration.
These filters can be combined using logical operators (``&``, ``|``, ``~``)
and standard comparison operators.

Supported Filters
-----------------
Filters must be used exactly as defined below. Any unsupported filter or
incorrect syntax will result in a policy configuration error.
We chose PascalCase keywords to avoid conflicts with Python's reserved words
(e.g., type).

- ``Path``: Full file path.
  Example: ``Path == "/ccc/fsname/scratch"``
  Matches files located exactly in the specified path.

- ``Name``: Case-sensitive file name.
  Example: ``Name == "report.txt"``
  Matches files with the exact name "report.txt".

- ``Iname``: Case-insensitive file name.
  Example: ``Iname == "report.txt"``
  Matches "report.txt", "REPORT.TXT", "Report.TxT", etc.

- ``Type``: File type, either ``"file"`` or ``"dir"``.
  Example: ``Type == "file"``
  Selects only regular files.

- ``Owner``: Username of the file owner.
  Example: ``Owner == "admin"``
  Matches files owned by the "admin" user.

- ``Group``: Group name of the file owner.
  Example: ``Group == "developers"``
  Matches files where the owning group is "developers".

- ``Size``: File size with units ``"K"``, ``"M"``, ``"G"``, ``"T"``.
  Example: ``Size >= "10G"``
  Selects files that are at least 10 GB in size.

- ``Dircount``: Number of subdirectories in a directory.
  Example: ``Dircount > 100``
  Selects directories containing more than 100 subdirectories.

- ``LastAccess``: Last access time, supporting relative values
  (``d`` = days, ``h`` = hours, ``m`` = minutes).
  Example: ``LastAccess > "30d"``
  Selects files not accessed in the last 30 days.

- ``LastModification``: Last modification time.
  Example: ``LastModification > "90d"``
  Selects files that have not been modified in the last 90 days.

- ``LastChange``: Last metadata change time.
  Example: ``LastChange > "60d"``
  Selects files whose metadata (permissions, owner, etc.)
  hasn't changed in the last 60 days.

- ``OstPool``: OST pool where the file is stored (for Lustre).
  Example: ``OstPool == "fast_pool"``
  Selects files stored in the OST pool named "fast_pool".

Supported Operators
-------------------
Comparison operators:
- ``==``  (equal to)
- ``!=``  (not equal to)
- ``>``   (greater than)
- ``>=``  (greater than or equal to)
- ``<``   (less than)
- ``<=``  (less than or equal to)

Logical operators:
- ``&``  (logical AND)
- ``|``  (logical OR)
- ``~``  (logical NOT)

Examples of Complex Conditions
------------------------------
Conditions can be combined to create advanced filtering rules.

**Example 1:**
Select files larger than 3GB, that are regular files, and haven't been accessed
in 180 days:

.. code:: python

    (Size >= "3G") & (Type == "file") & (LastAccess > "180d")

**Example 2:**
Select directories with more than 500 subdirectories OR owned by "admin":

.. code:: python

    (Type == "dir") & ((Dircount > 500) | (Owner == "admin"))

**Example 3:**
Select files not belonging to group "research" and modified in the last 30 days:
.. code:: python

    ~(Group == "research") & (LastModification < "30d")

Note on Logical Operators
-------------------------
If administrators prefer a more readable syntax using ``and``, ``or``,
and ``not``, it is important to note that Python does not allow overloading
these operators. To work around this limitation, an alternative approach
could be to write conditions as strings and this string can then be parsed and
evaluated by the policy engine. However, for the current implementation,
the syntax using ``&``, ``|``, and ``~`` should be used to ensure correct
behavior.

Example Policies
================

Implementation Overview
=======================

API and Available Functions
===========================

rbh-policy Command
==================

Execution Flow
==============

------------------------------------------------------------
End of Document
------------------------------------------------------------

