.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

Configuration Syntax
====================

RobinHood 4 Policy Engine uses a structured configuration format that allows
administrators to define:
- A global backend URI: identifies the target file system (e.g., Lustre).
- A global database URI: identifies the metrics database (e.g., MongoDB).
- Global operational parameters: trigger evaluation interval.
- Fileclasses: reusable conditions to match specific sets of files.
- Policies: defined set of rules consisting of its name, target,
  the action to be executed, and the trigger that activates it.

The configuration consists of three main declarations:
1. Global Backend URI Declaration: ``backend = "<uri>"``
2. Database URI Declaration: ``database = "<uri>"``
3. Global Evaluation Interval Declaration: ``evaluation_interval = "<duration>"``
4. Fileclasses Declaration: ``declare_fileclass``
5. Policy Declaration: ``declare_policy``

Backend and Database URI Declarations
-------------------------------------

Each full configuration must specify both a backend URI (target filesystem) and
a database URI (metadata source). These URIs allow the policy engine to
connect to the correct filesystem and metadata context.

These declarations are mandatory in the main configuration file, but may be
omitted from auxiliary files that only declare reusable components (e.g.,
fileclasses or policies shared across configurations).

**Syntax:**

.. code:: python

   backend = "rbh:<backend_type>:<fsname>"
   database = "rbh:<db_type>:<dbname>"

**Where:**

- ``rbh:`` — The RobinHood-specific URI scheme prefix (mandatory).
- ``<backend_type>``, ``<db_type>`` — The backend types (e.g., ``lustre``,
  ``mongo``).
- ``<fsname>``, ``<dbname>`` — Unique identifiers for the filesystem and database
  instances (e.g., ``lustre1``, ``prod_db``).

Notes:

- These declarations must appear once and only once in the main configuration
  file.
- They are not required in imported or auxiliary configuration fragments.
- Both URIs follow the same format.

Global Intervals Declaration
----------------------------

This mandatory parameter defines how frequently the daemon evaluates
metrics-based triggers using the current state of the database.

**Syntax:**

.. code:: python

   evaluation_interval = "5s"

Notes: This parameter is required and must appear exactly once in the
configuration.

Fileclasses Declaration
----------------------
A fileclass defines a set of files based on a combination of filters.
These file classes can be reused across multiple policies to avoid redundancy.

**Syntax:**

.. code:: python

    class_name = declare_fileclass(
        name = "<class_name>",
        target = <filter_condition>
    )

To enable reuse and introspection, each declared fileclass is typically assigned
to a variable. This allows policies to reference it directly by using this
variable name. The name provided in the ``name`` parameter is used internally by
the engine for logging, for exemple

**Example:**

Define a file class for large, old files:

.. code:: python

    exemple_fileclass = declare_fileclass(
        name = "exemple_fileclass",
        target = (Size >= "10GB") & (LastAccess > "180d")
    )

This file class matches all files that are larger than 10GB and were not
accessed in the last 180 days.

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
     A trigger is a logical expression that evaluates system metrics
     (user/group usage, Lustre pool usage, scheduling, etc.) and launches the
     policy when the condition is met.
     - Common triggers include:
       - Trigger when specified users exceed 1 million files
         ``trigger = UserFileCount("user42,user99") > 1_000_000``
       - Trigger when specified groups exceed 5 terabytes of storage
         ``trigger = GroupSize("groupA,groupB") > "5TB"``
       - Trigger when overall filesystem disk usage exceeds 90%
         ``trigger = GlobalSizePercent > "90%"``
       - Trigger when overall inode usage exceeds 90%
         ``trigger = GlobalInodePercent > "90%"``
       - Trigger automatically on a daily schedule
         ``trigger = Periodic("daily")``
       - Run the policy at a specific date and time
         ``trigger = Scheduled("2025-06-01 03:00")``
     - Additional triggers specific to Lustre include:
       - Trigger when the specified pools exceed 80% disk usage
         ``trigger = PoolSizePercent("data_pool1,data_pool2") > "80%"``
       - Trigger when the specified pools exceed 80% inode usage
         ``trigger = PoolInodePercent("data_pool1,data_pool2") > "80%"``
       - Trigger when specified OSTs exceed 85% disk usage
         ``trigger = OstSizePercent("ost_0,ost_1") > "85%"``
       - Trigger when specified OSTs exceed 85% inode usage
         ``trigger = OstInodePercent("ost_0,ost_1") > "85%"``

     - ClusterShell-style syntax is supported in function arguments:
       - ``UserFileCount("user[01-05]") > 10_000``

   - Multiple triggers can be defined using a list. The policy is triggered if
     any of the listed conditions is met.
     - Example:
       .. code:: python

        trigger = [
              GlobalSizePercent > "90%",
              Periodic("daily")
          ]
    - Triggers can also combine multiple conditions using ``&`` logical operator.
      - Example: trigger the policy when the filesystem usage exceeds 90%
        and inode usage exceeds 90%:

        .. code:: python

         trigger = GlobalSizePercent > "90%" & GlobalInodePercent > "90%"

   - Supported trigger functions:
     - ``UserFileCount(users)``: File count of one or more users
     - ``UserDiskUsage(users)``: Disk usage of one or more users
     - ``UserInodeCount(users)``: Inode count of one or more users
     - ``GroupFileCount(groups)``: File count of one or more groups
     - ``GroupSize(groups)``: Disk usage of one or more groups
     - ``GroupInodeCount(groups)``: Inode count of one or more groups
     - ``GlobalSizePercent``: Total filesystem disk usage (percentage)
     - ``GlobalInodePercent``: Total filesystem inode usage (percentage)
     - ``PoolSizePercent(pools)``: Lustre pool disk usage (percentage)
     - ``PoolInodePercent(pools)``: Lustre pool inode usage (percentage)
     - ``OstSizePercent(osts)``: OST disk usage (percentage)
     - ``OstInodePercent(osts)``: OST inode usage (percentage)
     - ``Periodic(freq)``: Run periodically (e.g., ``"daily"``, ``"hourly"``)
     - ``Scheduled(datetime)``: Run at a specific time (e.g.,
       ``"2025-06-01 03:00"``)

   - Controlled trigger execution:
     - In some cases, a policy should start when a threshold is exceeded,
       and stop when another threshold is reached.
       This is useful to gradually reduce usage rather than perform one-off
       actions.

       The ``ControlledTrigger`` function allows defining such behavior with a
       ``start`` and ``stop`` condition:
       - Example: run a purge policy when an OST exceeds 95% usage,
         and stop it once usage drops below 85%:

         ``trigger = ControlledTrigger(start = OstSizePercent("ost_0") > "95%",
                                       stop = OstSizePercent("ost_0") < "85%")``
     - Syntax:
       ``ControlledTrigger(start = <expression>, stop = <expression>)``

     - Both ``start`` and ``stop`` must be valid trigger expressions.

   - Note: More complex triggers may be implemented in the future. Feel free
     to suggest additional trigger conditions if needed.

5. ``parameters`` (optional):
   - A dictionary of key-value pairs defining additional parameters for the
     action.
   - Example:
     - ``parameters = {"compression": "gzip", "nb_threads": 5}``

6. ``rules`` (optional):
   - Rules are used to apply different actions to specific subsets of the
     policy's target. Each rule must specify its own target subset, and may
     override the action and parameters.
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
            "schedulers": common.rate_limit,
            "rate_limit": {
                "max_count": 50,
                "period_ms": 1000
            }
        },
        trigger = Periodic("10m"),
        rules = [
            Rule(
                condition = Owner == "root" | Owner == "nfsnobody" | work |
                            somegroup,
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
- Configuring parameters such as thread count, error suspension, and rate
  limiting.
- Automatically triggering every 10 minutes.
- Ignoring files owned by ``root`` or ``nfsnobody``, as well as files matching
  ``work`` or ``somegroup`` fileclasses.
- Cleaning up files older than 60 days based on last access and creation time.
