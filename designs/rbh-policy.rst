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
actions on storage element based on rules defined by administrators.
This document describes the functionality, configuration syntax, and high-level
implementation of the policy engine.
The engine is executed via the ``rbh-policy`` command, which takes the
configuration file and the name of the policy to execute as arguments.
A daemon will be added later to manage the execution of policies automatically.
We will have two execution modes:
- The first is to execute a policy explicitly by specifying its name via the
  ``rbh-policy`` command. This allows administrators to force the execution of
  a policy at any time, based on the trigger.
- The second is to let the daemon automatically execute policies based on
  defined triggers.
In both cases, policies and their behavior are defined entirely in the
configuration file.

Python has been chosen as the configuration language because it provides the
flexibility of a full-fledged programming language. This allows administrators
to easily define and understand policies, while also enabling them to implement
advanced or unforeseen behaviors directly within the configuration file, if
desired. Unlike traditional configuration formats such as YAML or JSON, Python
eliminates the need for additional scripting (e.g., Bash scripts) by offering
all the tools and capabilities needed within a single environment.
Additionally, having the entire system, from the configuration to the policy
engine and the daemon built in Python ensures seamless integration and provides
a consistent and unified development environment.

Goals
=====
* Provide administrators with a simple and clear configuration interface to
  define and manage policies.
* Enable file grouping via reusable file classes based on criteria.
* Automate the execution of actions on files based on administrator-defined
  rules.
* Allow administrators to incorporate advanced Python behaviors in their
  configuration for customized functionality.
* Support orchestration and automation of policy execution with a daemon,
  ensuring streamlined and efficient management.

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
        condition = (Size >= "10GB") & (Last_Access > "180d")
    )

This file class matches all files that are larger than 10GB and not accessed in
the last 180 days.

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
        trigger = <trigger_condition>,
        parameters = {
            <optional_action_parameters>
        },
        rules = [
            {
                "name": "<rule_name>",
                "condition": <fileclass_condition | expression>,
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
       ``(Size >= 10MB) & (Last_Access > 180d)``.
     - You can also combine file attributes with fileclass references, e.g.,
       ``(Size >= 10MB) & (Last_Access > 180d) & recent_files``.

3. ``action`` (mandatory):
    - Specifies the function or command to execute when the policy applies.
    - Can be a predefined function from the RobinHood API.
    - Or a custom Python function defined by the administrator.
    - Can also be an external command or shell command.
    - Example:
      - ``action = archive_files`` (predefined function to archive files)
      - ``action = custom_action_function`` (custom Python function)
      - ``action = cmd(f"archive_tool --path {fullpath}")`` (external command)

4. ``trigger`` (mandatory):
   - Defines a condition under which the policy is automatically executed.
     - Common triggers include:
       - Trigger when specified users exceed 1 million files
         ``{ "UserUsage": ["user42", "user99"], "Threshold": ">1M files" }``
       - Trigger when specified groups exceed 5 terabytes of storage
         ``{ "GroupUsage": ["groupA", "groupB"], "Threshold": ">5TB" }``
       - Trigger when overall filesystem usage exceeds 90%
         ``{ "GlobalUsage": ">90%" }``
       - Trigger automatically on a daily schedule
         ``{ "Periodic": "daily" }``
       - Run the policy at a specific date and time
         ``{ "Scheduled": "2024-06-01 03:00" }``
     - Additional triggers specific to Lustre include:
       - Trigger when the specified pools exceed 80% usage
         ``{ "PoolUsage": ["data_pool1", "data_pool2"], "Threshold": ">80%" }``
       - Trigger when specified OSTs exceed 85% usage
         ``{ "OstUsage": ["ost_0", "ost_1"], "Threshold": ">85%" }``
    - Note: More complex triggers may be implemented in the future. Feel free
       to suggest additional trigger conditions if needed.

5. ``parameters`` (optional):
   - A dictionary of key-value pairs defining additional parameters for the
     action.
   - Example:
     - ``parameters = {"compression": "gzip", "nb_threads": 5}``

6. ``rules`` (optional):
   - A list of rules that apply to subsets of the policy's target. Each rule can
     override specific details of the policy’s action, such as the target files,
     the action to be executed, and any action parameters.
   - Each rule includes:
     - ``name``: A unique string identifier for the rule
       (e.g., ``"archive_recently_modified"``).
     - ``condition``: Defines a subset of the main policy target. This can be a
       specific fileclass or condition that further refines the files this rule
       applies to.
     - ``action`` (optional): Overrides the default action for this rule,
       allowing different actions for different conditions.
     - ``parameters`` (optional): Overrides the action parameters for this
       specific rule.
   - **Note:** Rules are applied in the order they appear in the configuration.
     When a file matches the condition of a rule, it applies the action of that
     rule and skips subsequent rules. If no rules match, the default policy
     action is applied.

**Example: Migration of the "cleanup" policy from RBH3 to RBH4**

.. code:: python

    declare_policy(
        name = "cleanup",
        target = (Type == "file"),
        action = cmd("/usr/sbin/rbh_cleanup_trash.sh /tmp/scratch {path}"),
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
                "condition": Owner == "root" | Owner == "nfsnobody",
                "action": None
            },
            {
                "name": "ignore_work_fileclass",
                "condition": work,
                "action": None
            },
            {
                "name": "ignore_somegroup_fileclass",
                "condition": somegroup,
                "action": None
            },
            {
                "name": "default_cleanup",
                "condition": (LastAccess > "60d"),
                "action": None
            }
        ]
    )

This policy defines file cleanup in the scratch filesystem by:
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
  Example: ``Path == "/tmp/scratch"``
  Matches files located exactly in the specified path.

- ``Name``: Case-sensitive file name.
  Example: ``Name == "report.txt"``
  Matches files with the exact name "report.txt".

- ``Iname``: Case-insensitive file name.
  Example: ``Iname == "report.txt"``
  Matches "report.txt", "REPORT.TXT", "Report.TxT", etc.

- ``Type``: File type, either ``"file"``, ``"dir"`` or ``"symlink"``.
  Example: ``Type == "file"``
  Selects only regular files.

- ``Owner``: Username of the file owner.
  Example: ``Owner == "admin"``
  Matches files owned by the "admin" user.

- ``Group``: Group name of the file owner.
  Example: ``Group == "developers"``
  Matches files where the owning group is "developers".

- ``Size``: File size with units ``"KB"``, ``"MB"``, ``"GB"``, ``"TB"``.
  Example: ``Size >= "10GB"``
  Selects files that are at least 10 GB in size.

- ``Dircount``: Total number of elements in a directory.
  Example: ``Dircount > 100``
  Selects directories containing more than 100 elements.

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

Storage and Time Units
----------------------
Storage units:
- ``B``   (bytes)
- ``KB``  (kilobytes)
- ``MB``  (megabytes)
- ``GB``  (gigabytes)
- ``TB``  (terabytes)
- ``%``   (percentage)

Large quantity units:
- ``k``   (thousands)
- ``M``   (millions)
- ``T``  (billions)

Time units:
- ``s``   (seconds)
- ``m``   (minutes)
- ``h``   (hours)
- ``d``   (days)

Examples of Complex Conditions
------------------------------
Conditions can be combined to create advanced filtering rules.

**Example 1:**
Select files larger than 3GB, that are regular files, and haven't been accessed
in 180 days:

.. code:: python

    (Size >= "3GB") & (Type == "file") & (LastAccess > "180d")

**Example 2:**
Select directories with more than 500 subdirectories OR owned by "admin":

.. code:: python

    (Type == "dir") & ((Dircount > 500) | (Owner == "admin"))

**Example 3:**
Select items that are not files and modified in the last 30 days:
.. code:: python

    ~(Type == "file") & (LastModification < "30d")

Note on Logical Operators
-------------------------
If administrators prefer a more readable syntax using ``and``, ``or``,
and ``not``, it is important to note that Python does not allow overloading
these operators. To work around this limitation, an alternative approach
could be to write conditions as strings and this string can then be parsed and
evaluated by the policy engine. However, for the current implementation,
the syntax using ``&``, ``|``, and ``~`` should be used to ensure correct
behavior.

Implementation Overview
=======================

Communication between the Configuration File and the Program
-----------------------------------------------------------

The daemon arriving in a second time, we are talking here about the manual
execution of the policy using the ``rbh-policy`` command.
At the start of execution, the RobinHood 4 Policy Engine needs to load the
configuration file in order to define the policies and fileclasses that will be
used. The process is designed to be simple and flexible, ensuring that
administrators can easily specify their own policies and actions in a
Python-based configuration file.

1. **Loading the Configuration File**

The engine takes the path to the configuration file as a command-line argument.
It then loads this file dynamically as a Python module. This is not a standard
config parsing, it’s more like importing a Python script at runtime.

2. **Making the Connection Possible**

To allow the configuration file to define fileclasses and policies using the
functions ``declare_fileclass`` and ``declare_policy``, the program injects
those functions (along with useful constant and fonction like actions) into the
execution context of the config file.
This way, when the configuration file runs and calls ``declare_policy(...)``,
it’s actually calling the engine’s internal function, which stores the policy
inside a dictionary for later use. The same goes for fileclasses and other
elements.

In addition, if the configuration file defines new things, for example a custom
action function, the engine also has access to it. Since the configuration is a
Python script that runs in a known execution context, the program can retrieve
any custom function, variable, or class defined inside. These can then be used
during policy execution just like built-in components.

The config is not just a static declaration, it can include real Python logic
that becomes part of how the engine works during execution.

3. **Selecting the Target Policy**

After the configuration file has finished running, all fileclasses and policies
are now stored in internal dictionaries. The engine looks into the dictionary of
registered policies to find the one that matches the name provided by the user
on the command line.

4. **Handling Errors and Validations**

The engine currently relies on Python's own error handling to detect
misconfigurations in the file. In future versions, additional checks or
validations could be added to provide more user-friendly error messages or
custom validations.

Building Conditions in the Configuration File
---------------------------------------------

In configuration files, conditions are written using standard Python expressions.

For example:

    (Type == "dir") & ((Dircount > 500) | (Owner == "admin"))

This expression uses Python comparison operators (``==``, ``>``, ``<``) and
bitwise logical operators (``&`` for AND, ``|`` for OR, ``~`` for NOT).

Although it looks like normal Python, these comparisons and operators are not
working on regular values. Instead, each condition (like ``Type == "dir"``)
returns an internal object representing the condition itself. The logical
operators then combine them into a more complex structure.

This is possible because all the comparison and operator functions
(like ``__eq__``, ``__and__``, etc.) are overloaded internally. So when you
write ``Size > 100``, it builds a condition object, not a boolean.

The final condition expression is stored as part of the policy or fileclass,
and will be evaluated later on real file entries.

This system makes it possible to write clear and expressive filters using
familiar Python syntax, without needing a custom language or parser.

Policy Execution Flow
---------------------

Once the configuration has been loaded and a policy selected, the execution
phase begins.

This section describes how a policy is executed manually using the ``rbh-policy``
command (i.e., outside of the daemon, which is responsible for automatic
execution based on triggers).

1. **Looking Up the Policy**

The user specifies the policy name as a command-line argument. The engine checks
whether a policy with this name exists in the internal dictionary populated
during config loading.

If the policy is not found, an error is returned. Otherwise, the corresponding
policy object is retrieved and used for execution.

2. **Filtering Entries Based on the Target Criteria**

The policy’s target defines a global condition (e.g., ``Size > 100MB``) that
determines which entries are eligible for processing. This condition is used as
the base for all evaluations during the execution of the policy.

However, the engine does not perform a single global filtering first and then
refine. Instead, it incorporates the target condition into every subsequent rule
evaluation, ensuring that all entries match the policy’s scope.

3. **Progressive Rule Evaluation and Exclusion Strategy**

Rules are applied in the order they are defined in the configuration file.
For each rule:

- The engine builds a composite condition combining:
  - The policy’s global target condition,
  - The rule’s specific condition (e.g., ``Size > 150MB & LastAccess > 180d``),
  - And a negation of all previously matched rule conditions.

This ensures that entries already matched and processed by earlier rules are
excluded from the current rule’s evaluation.

For example:

If the policy target is ``Size > 100MB``, and we have the following rules:

  - Rule A: ``Size > 150MB & LastAccess > 180d``
  - Rule B: ``Size == 600MB``

The evaluation proceeds as follows:

1. For Rule A: The engine evaluates a condition matching
   ``Size > 100 & Size > 150 & LastAccess > 180d``
   - Matching entries are processed using the action defined in Rule A.

2. For Rule B: The engine evaluates entries matching
   ``Size > 100 & ~(Size > 150 & LastAccess > 180d) & Size == 600``
   - Matching entries are processed using Rule B's action.

3. For remaining entries: If a default action is defined, it is applied to
   entries matching
   ``Size > 100 & ~(Size > 150 & LastAccess > 180d) & ~(Size == 600)``

This strategy guarantees:

- Only one rule applies per entry (the first one that matches).
- No entry is processed more than once.
- Rules are prioritized by their order of appearance.

4. **Executing Actions and Parameters**

When a rule matches an entry, its associated action is executed according to the
following logic:

- If the rule explicitly defines an action, this action replaces the default
  action of the policy and is used for the matching entries.
- If the rule does not define a new action but provides action parameters, then
  the default action from the policy is used, but the parameters are overridden
  or extended by those defined in the rule.
- If the rule specifies neither an action nor parameters, the policy's default
  action and parameters are applied and this rules is useless.

To explicitly ignore certain entries, a rule can set its ``action`` field to
``none``. This is useful when you want to skip processing for specific subsets
of files without needing to define a separate policy.

5. **Logging Execution Details**

During the execution of a policy, the Policy Engine provides detailed logging to
make its behavior transparent and traceable.

For each policy run, the following information is logged:
- The ``name of the policy`` currently being executed.
- The ``rules`` being evaluated and their associated conditions.
- For each rule that matches entries, the engine logs:
  - The ``action`` applied.
  - The ``entries`` affected by this rule.

- If a rule does not match any entries, this is also indicated in the logs.
- If an entry does not match any rule and the default action is used, this is
  also explicitly logged.

In the case of any error during execution (e.g., malformed condition, failed
external command, missing parameters), the error is logged with enough detail to
understand.

At the end of execution, a summary report is printed with aggregated information
such as:
- Total number of entries processed.
- Number of entries per rule (including those with action ``none``).
- Whether the default rule was used.
- Total number of errors (if any).
- Total execution time.

This logging mechanism ensures administrators can track the policy execution
process step by step, and easily identify configuration issues or unexpected
behaviors.

API and Available Functions
===========================

rbh-policy Command
==================

Execution Flow
==============

------------------------------------------------------------
End of Document
------------------------------------------------------------
