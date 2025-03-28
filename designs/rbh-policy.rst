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
actions on storage elements based on rules defined by administrators.
This document describes the functionality, configuration syntax, and high-level
implementation of the policy engine.
The engine is executed via the ``rbh-policy`` command, which takes the name of
the policy to execute as arguments.
A daemon will be added later to manage the execution of policies automatically.
We will have two execution modes:
- The first is to execute a policy explicitly by specifying its name via the
  ``rbh-policy`` command. This allows administrators to force the execution of
  a policy at any time, based on the trigger.
- The second is to let the daemon automatically execute policies based on
  defined triggers.
In both cases, policies and their behavior are defined entirely in the
configuration file.

Configuration is organized per filesystem. By default, configuration files are
stored in a standard directory (e.g., ``/etc/robinhood4.d/``) and named after
the target filesystem (e.g., ``fs1.py`` for filesystem ``fs1``). This allows the
policy engine to automatically locate the right configuration file when only the
filesystem name is provided.

An optional ``--config <file.py>`` argument allows administrators to provide an
additional configuration file. This secondary file is loaded alongside the
default one and may define extra policies or override specific behaviors without
altering the main configuration.

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

In this document we deal with files but this will handle any type of storage
elements

Goals
=====
* Provide administrators with a simple and clear configuration interface to
  define and manage policies.
* Enable file grouping via reusable file classes based on criteria.
* Automate the execution of actions on files based on administrator-defined
  rules.
* Allow administrators to incorporate advanced behaviors in their
  configuration for customized functionality.
* Support orchestration and automation of policy execution with a daemon,
  ensuring streamlined and efficient management.
* Ensure scalability and high performance, even on large-scale filesystems,
  by minimizing overhead and optimizing trigger evaluation and file selection.

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
        condition = (Size >= "10GB") & (LastAccess > "180d")
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
            Rule(
                name = "<rule_name>",
                condition = <fileclass_condition | expression>,
                action = <optional_override_action>,
                parameters = {
                    <optional_override_parameters>
                }
            ),
            ...
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
       ``(Size >= "10MB") & (Last_Access > "180d")``.
     - You can also combine file attributes with fileclass references, e.g.,
       ``(Size >= "10MB") & (Last_Access > "180d") & recent_files``.

3. ``action`` (mandatory):
    - Specifies the function or command to execute when the policy applies.
    - Can be a predefined function from the RobinHood API.
    - Or a custom Python function defined by the administrator.
    - Can also be an external command or shell command.
    - Example:
      - ``action = archive_files`` (predefined function to archive files)
      - ``action = custom_action_function`` (custom Python function)
      - ``action = cmd("archive_tool --path {fullpath}")`` (external command)

4. ``trigger`` (mandatory):
   - Defines a condition under which the policy is automatically executed.
     - Common triggers include:
       - Trigger when specified users exceed 1 million files
         ``trigger = (UserUsage == ["user42", "user99"]) &
           (FileCount == 1_000_000)``
       - Trigger when specified groups exceed 5 terabytes of storage
         ``trigger = (GroupUsage == ["groupA", "groupB"]) & (Size > "5TB")``
       - Trigger when overall filesystem usage exceeds 90%
         ``trigger = (GlobalUsage > "90%")``
       - Trigger automatically on a daily schedule
         ``trigger = (Periodic == "daily")``
       - Run the policy at a specific date and time
         ``trigger = (Scheduled == "2024-06-01 03:00")``
     - Additional triggers specific to Lustre include:
       - Trigger when the specified pools exceed 80% usage
         ``trigger = (PoolUsage == ["data_pool1", "data_pool2"]) & (Usage >
           "80%")``
       - Trigger when specified OSTs exceed 85% usage
         ``trigger = (OstUsage == ["ost_0", "ost_1"]) & (Usage > "85%")``
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
   - Note: Rules are applied in the order they appear in the configuration.
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
        trigger = (Periodic == "10m"),
        rules = [
            Rule(
                condition = Owner == "root" | Owner == "nfsnobody",
                action = None
            ),
            Rule(
                condition = work,
                action = None
            ),
            Rule(
                condition = somegroup,
                action = None
            ),
            Rule(
                condition = (LastAccess > "60d"),
                action = None
            )
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

- ``Path``: Full file path (supports regular expressions).
  Example: ``Path == "/tmp/scratch/cout*/*/"``
  Matches files located exactly in the specified path.

- ``Name``: Case-sensitive file name.
  Example: ``Name == "report.txt"``
  Matches files with the exact name "report.txt".

- ``IName``: Case-insensitive file name.
  Example: ``IName == "report.txt"``
  Matches "report.txt", "REPORT.TXT", "Report.TxT", etc.

- ``Type``: File type, either ``"file"``, ``"dir"`` or ``"symlink"``.
  Example: ``Type == "file"``
  Selects only regular files.

- ``Owner``: Username of the file owner.
  Example: ``Owner == "admin"``
  Matches files owned by the "admin" user.

- ``UID``: Numeric user ID of the file owner.
  Example: ``UID == 1001``
  Matches files owned by the user with UID 1001.

- ``Group``: Group name of the file owner.
  Example: ``Group == "developers"``
  Matches files where the owning group is "developers".

- ``Size``: File size with units ``"KB"``, ``"MB"``, ``"GB"``, ``"TB"``.
  Example: ``Size >= "10GB"``
  Selects files that are at least 10 GB in size.

- ``DirCount``: Total number of elements in a directory.
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
- ``T``   (billions)

Time units:
- ``s``   (seconds)
- ``m``   (minutes)
- ``h``   (hours)
- ``d``   (days)
- ``w``   (weeks)
- ``M``   (months)

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
Select entries that do not belong to the 'recent_files' fileclass, do not exceed
10MB in size, and have not been accessed in the last 180 days.

.. code:: python

    ~recent_files & (Size < "10MB") & (LastAccess < "180d")

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

Executing and Interpreting the Configuration File
-------------------------------------------------

The daemon arriving in a second time, we are talking here about the manual
execution of the policy using the ``rbh-policy`` command.
At first, the RobinHood 4 Policy Engine needs to import the configuration file
to retrieve the policies and fileclasses previously defined by the
administrator. The process is designed to be simple and flexible, allowing users
to specify their own policies and actions directly in a Python-based
configuration file.

1. **Loading the Configuration File**

The engine takes the path to the configuration file as a command-line argument.
It then imports this file as a Python module at runtime.
This is not a standard static config parsing: instead, the file is executed in
a pre-filled environment where all the necessary functions and variables (such
as declare_policy, Size, etc.) are already available.
This allows administrators to write policies in plain Python without needing to
explicitly import anything.

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
misconfigurations in the file. Python will raise errors for issues such as:
- Syntax errors (e.g., incorrect indentation, unmatched parentheses)
- Missing arguments in function calls (e.g., calling ``declare_policy()``
  without the required parameters)
- Usage of undefined variables (e.g., referencing a fileclass that hasn't been
  declared)

Logical errors that do not raise exceptions directly, such as mismatched
parameter types for actions, are currently not detected by Python itself and may
result in runtime failures. In future versions, additional checks could be
implemented to catch these issues, providing clearer error messages and better
validation. For example:
   - Ensuring that rules have valid `target` conditions
   - Ensuring that policy actions and parameters are compatible with each other
   - Reporting when a policy is defined with a duplicate name
   ...

Building Conditions in the Configuration File
---------------------------------------------

In configuration files, conditions are written using standard Python
expressions.

For example:
.. code:: python

    (Type == "dir") & ((Dircount > 500) | (Owner == "admin"))

This expression uses Python comparison operators (``==``, ``>``, ``<``) and
bitwise logical operators (``&`` for AND, ``|`` for OR, ``~`` for NOT).

Although it looks like normal Python, these comparisons and operators are not
working on regular values. Instead, each condition (like ``Type == "dir"``)
returns an internal object representing the condition itself. The logical
operators then combine them into a more complex structure.

This is possible because all the comparison and operator functions
(like ``__eq__``, ``__and__``, etc.) are overloaded internally. So when you
write ``Size > "100MB"``, it builds a condition object, not a boolean.

The final condition expression is stored as part of the policy or fileclass,
and will be evaluated later on real file entries.

This system makes it possible to write clear and expressive filters using
familiar Python syntax, without needing a custom language or parser.

Policy Execution Flow
---------------------

Once the configuration has been loaded and a policy selected, the execution
phase begins.

This section describes how a policy is executed manually using the
``rbh-policy`` command (i.e., outside of the daemon, which is responsible for
automatic execution based on triggers).

1. **Looking Up the Policy**

The user specifies the policy name as a command-line argument. The engine checks
whether a policy with this name exists in the internal dictionary populated
during config loading.

If the policy is not found, an error is returned. Otherwise, the corresponding
policy object is retrieved and used for execution.

2. **Validating and Evaluating the Trigger**

Once the policy is selected, its associated trigger is immediately evaluated.
Triggers are mandatory and define under which circumstances a policy is
intended to run. Even in manual execution mode, this evaluation is not skipped.

The trigger evaluation behaviour differs depending on the execution context:
- ``Manual execution`` (via ``rbh-policy``):
  The engine starts by filtering entries based on the default target condition
  of the policy. The trigger is then evaluated against this filtered set.
  If the trigger is satisfied, the engine proceeds with the full evaluation
  and execution of the policy (including rules and actions). If not, the
  execution stops at this stage.

- ``Scheduled execution`` (via daemon):
  The daemon performs regular scans of the system to evaluate all triggers
  across policies. These scans are independent of any policy's specific
  filtering rules. If a trigger is satisfied, the corresponding policy is
  selected and executed including a new evaluation of its target and rules.

For backend-specific triggers (e.g., Lustre pool or OST usage), the engine
retrieves usage metrics directly through the Lustre API. This backend
integration is modular and can be extended to support other filesystems or new
trigger types.

3. **Filtering Entries Based on the Target Criteria**

The policy’s target defines a global condition (e.g., ``Size > "100MB"``) that
determines which entries are eligible for processing. This condition is used as
the base for all evaluations during the execution of the policy.

At this stage, the engine performs one of two possible approaches for processing
the entries:

- **Approach 1: Per-Rule Filtering (No Filesystem Access)**
  In this approach, for each rule, the engine checks whether the entry matches
  the rule's condition directly against the database, combining it with the
  global target condition. There is no access to the filesystem during this step
  as all evaluations are done using metadata stored in the database. This avoids
  file system I/O operations.

  In this approach:
  - Each rule is independently evaluated, using the global target condition
    combined with the rule's specific condition.
  - For each rule, the condition is evaluated using only the database's
    metadata, and any entry matching the rule's condition is processed.
  - The negation of the previously matched rule conditions ensures that once an
    entry is processed by a rule, it is excluded from subsequent rule
    evaluations.

- **Approach 2: Global Filtering and Filesystem Access**
  In this approach, the engine first performs a single global query to the
  database that matches the target condition of the policy
  (e.g., ``Size > 100MB``). This query retrieves a list of entries that satisfy
  the base condition of the policy.

  Once the entries are retrieved, each entry is evaluated against the rules in
  the order they are defined:
  - For each rule, the entry is checked to see if it satisfies the rule’s
    condition.
  - If the entry satisfies a rule, the corresponding action is applied.
  - If the entry does not satisfy the rule, it is checked against the next rule.
  - If no rule matches, the default action of the policy is applied to the
    entry.
  Filesystem access is performed for each entry during this process to retrieve
  necessary information that may not be available in the database
  (e.g., checking specific filesystem attributes like the last access time).

4. **Progressive Rule Evaluation and Exclusion Strategy**
Regardless of the approach used, the engine evaluates the rules in the order
they are defined in the configuration file. For each rule:

- **Approach 1: Per-Rule Filtering**
  The engine evaluates each rule independently. For each rule:
  - The engine builds a composite condition combining:
    - The policy’s global target condition,
    - The rule’s specific condition (e.g., ``Size > 150MB & LastAccess > 180d``)
    - And a negation of all previously matched rule conditions.
  This ensures that entries already matched and processed by earlier rules are
  excluded from the current rule’s evaluation.

  For example, if the policy target is ``Size > 100MB``, and we have the
  following rules:
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

- **Approach 2: Global Filtering and Filesystem Access**
  The engine begins by executing a single global query on the metadata database
  to retrieve a list of entries matching the policy’s target condition
  (e.g., ``Size > 100MB``). This forms the input set of entries eligible for
  rule evaluation.

  Then, for each entry in this list, the engine evaluates the rules
  sequentially, in the order they are defined in the policy configuration:

  1. The engine checks if the entry satisfies the condition of the first rule.
     If it matches, the corresponding action is executed, and the engine
     immediately proceeds to the next entry.
  2. If the entry does not match the first rule, the engine evaluates the next
     rule.
  3. This process continues until a rule matches.
  4. If no rule matches, the policy's default action is applied to the entry.

  For exemple, if the policy target is ``Size > 100MB``, and the following
  rules:
    - Rule A: ``Size > 150MB & LastAccess > 180d``
    - Rule B: ``Size == 600MB``

  The process is as follows:
  1. Global Filtering: Query the metadata database for entries matching
     ``Size > 100MB``.
  2. Per-Entry Rule Evaluation:
     - For each entry:
       - Check if it matches Rule A:
         - If yes, execute Rule A's action, and skip further evaluation for
           this entry.
         - If not, check Rule B.
       - If it matches Rule B:
         - Execute Rule B's action.
       - If it does not match any rule:
         - Apply the policy’s default action.

  This strategy guarantees:
  - Only the first matching rule applies to each entry.
  - No entry is processed more than once.
  - Rule priority is respected by evaluation order.

5. **Executing Actions and Parameters**

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
``None``. This is useful when you want to skip processing for specific subsets
of files without needing to define a separate policy. When the action field is
set to None, the corresponding entries are excluded entirely from processing,
including retrieval. The engine ensures that these ignored entries are excluded
by adding the opposite criterion to the filtering conditions for subsequent
rules.

6. **Logging Execution Details**

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
- Number of entries per rule (including those with action ``None``).
- Whether the default rule was used.
- Total number of errors (if any).
- Total execution time.
- Average processing rate (entries per second).

This logging mechanism ensures administrators can track the policy execution
process step by step, and easily identify configuration issues or unexpected
behaviors.

rbh-policy Command
==================

The ``rbh-policy`` command is the main entry point to interact with the
RobinHood 4 Policy Engine. It supports two main execution modes:

- Manual execution of one or more policies (via ``rbh-policy``).
- Daemon mode for automatic and continuous policy monitoring based on triggers.

This command-line interface acts as a unified tool for both administrators and
automation systems to interact with the policy engine.

Manual Mode
-----------

In manual mode, policies are explicitly executed from the command line.
The engine loads the configuration file(s), evaluates the trigger for each
selected policy, and if the trigger is satisfied, executes the associated rules
and actions.

.. code:: bash

   rbh-policy fs1 <policy1>[,<policy2>,...] [options]

Arguments:
- ``fs1``: The target filesystem name (used to locate
           ``/etc/robinhood4.d/fs1.py``)
- ``<policy1>,<policy2>,...``: One or more policy names declared in the config.
- ``all``: Special keyword to execute all policies defined in the file.

Available options:
- ``--config <file.py>``: An additional configuration file.
- ``--dry-run``: Simulates execution without applying any action.
- ``--verbose``: Enables detailed output, including matched entries and rules,
                 and logs actions performed.

Each policy name may optionally include parameters in the format:
``policyname(arg1,arg2,...)``

This syntax is inherited from RobinHood v3 and allows runtime control over how
a policy is executed, without modifying the configuration file.

**Supported parameters:**

- ``target=<tgt>``: Limits the scope of entries processed by the policy.
  Available targets:
  - ``all``: All entries matching the policy scope.
  - ``user:<username>``: Only entries belonging to the specified user.
  - ``group:<groupname>``: Only entries belonging to the specified group.
  - ``file:<path>``: A specific file or directory.
  - ``class:<fileclass>``: Entries matching the given fileclass.
  - ``ost:<ost_idx>``: Entries located on the specified OST index.
  - ``pool:<poolname>``: Entries located in the given OST pool.

- ``max-count=<nbr>``: Maximum number of actions to perform during this
                       execution.

- ``max-vol=<size>``: Maximum total volume of data to process (e.g., ``1TB``).

- ``target-usage=<pct>``: Reduces usage to the specified target.
  If usage is currently 80% and ``target-usage=75``, the engine processes
  enough entries to bring usage down to 75%.

**Dry-Run Behavior**

When the ``--dry-run`` option is used, the engine performs all the steps of
a normal policy execution except for running the actual actions.

This includes:
- Loading the configuration and policy definitions.
- Filtering filesystem entries based on each policy’s target.
- Evaluating triggers.
- Applying rule matching and selection logic.

Instead of executing actions, the engine generates a simulation report with:
- Number of entries matched by each rule.
- The action that would have been executed.
- The list of rules that matched nothing.
- A list of matching entries per rule:
  - A short sample in standard mode.
  - A full or extended list in ``--verbose`` mode.

This is particularly useful for:
- Validating new policies.
- Understanding rule behavior.
- Debugging configuration errors without modifying any data.

**Examples:**

.. code:: bash

   # Run the archive policy on all entries in pool0, up to 1TB
   rbh-policy fs1 archive(target=pool:"pool0",max-vol="1TB")

   # Execute the cleanup policy for user 'alice', max 500 entries
   rbh-policy fs1 cleanup(target=user:alice,max-count=500)

   # Run both cleanup and archive with distinct parameters
   rbh-policy fs1 cleanup,archive(target-usage=75)

   # Simulate cleanup policy, verbose output
   rbh-policy fs1 cleanup --dry-run --verbose

   # Run all policies defined in the configuration for fs1
   rbh-policy fs1 all

   # Run both cleanup and archive policies with default parameters
   rbh-policy fs1 cleanup,archive

Daemon Mode
-----------

In daemon mode, the engine continuously evaluates the triggers of the selected
policies. When a trigger is satisfied, the corresponding policy is executed
automatically.

This mode is designed to be used primarily with a systemd service. The system
administrator does not need to invoke the daemon manually via the CLI. Instead,
systemd runs the ``rbh-policy --daemon`` command in the background, using a
configuration file associated with each filesystem.

To specify additional parameters such as which policies to monitor, it is
recommended to use environment variables passed to the systemd unit.

**Example (systemd service):**

.. code:: ini

   ExecStart=/usr/bin/rbh-policy --daemon %i $RBH_POLICIES

Where:

- ``%i`` is the filesystem name (e.g., `fs1`)
- ``$RBH_POLICIES`` can be set via an environment file:
  ``RBH_POLICIES="cleanup,archive"``

This setup allows the administrator to control which policies the daemon
monitors per filesystem, while keeping systemd in charge of process supervision
and restarts.

**CLI usage (for testing or prototyping):**

Though intended primarily for systemd integration, the daemon can also be
started manually for development or debugging:

.. code:: bash

   # Start daemon for all policies
   rbh-policy --daemon fs1 all

   # Start daemon for selected policies only
   rbh-policy --daemon fs1 cleanup,archive

   # Start with detailed output
   rbh-policy --daemon fs1 all --verbose

Arguments:
- ``--daemon``: Enables daemon mode.
- ``<policy1>,<policy2>,...``: One or more policy names declared in the config.
- ``all``: Special keyword to monitor all policies defined in the file.

Available options:
- ``--config <file.py>``: An additional configuration file.
- ``--verbose``: Enables detailed output during daemon execution, including
  matched entries, triggered rules, and executed actions.

Exit Codes
----------

- ``0``: Success
- ``1``: Invalid configuration or arguments
- ``2``: Runtime or execution error

Daemon Overview
===============

The daemon in RobinHood 4 runs in the background to automatically enforce
policies based on triggers. Each trigger defines how and when to evaluate
conditions, allowing the engine to react promptly without scanning the entire
system or requiring manual intervention.

Launching the Daemon
--------------------

Base API and Extensibility
==========================

Execution Flow
==============

------------------------------------------------------------
End of Document
------------------------------------------------------------
