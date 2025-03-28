.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

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
     A trigger is a logical expression that evaluates system metrics
     (user/group usage, Lustre pool usage, scheduling, etc.) and launches the
     policy when the condition is met.
     - Common triggers include:
       - Trigger when specified users exceed 1 million files
         ``trigger = UserFileCount("user42,user99") > 1_000_000``
       - Trigger when specified groups exceed 5 terabytes of storage
         ``trigger = GroupSize("groupA,groupB") > "5TB"``
       - Trigger when overall filesystem usage exceeds 90%
         ``trigger = GlobalUsage > "90%"``
       - Trigger automatically on a daily schedule
         ``trigger = Periodic("daily")``
       - Run the policy at a specific date and time
         ``trigger = Scheduled("2025-06-01 03:00")``
     - Additional triggers specific to Lustre include:
       - Trigger when the specified pools exceed 80% usage
         ``trigger = PoolUsage("data_pool1,data_pool2") > "80%"``
       - Trigger when specified OSTs exceed 85% usage
         ``trigger = OstUsage("ost_0,ost_1") > "85%"``

     - ClusterShell-style syntax is supported in function arguments:
       - ``UserFileCount("user[01-05]") > 10_000``

   - Supported trigger functions:
     - ``UserFileCount(users)``: File count of one or more users
     - ``UserSize(users)``: Disk usage of one or more users
     - ``GroupFileCount(groups)``: File count of one or more groups
     - ``GroupSize(groups)``: Disk usage of one or more groups
     - ``PoolUsage(pools)``: Usage percentage of one or more Lustre pools
     - ``OstUsage(osts)``: Usage percentage of one or more Lustre OSTs
     - ``GlobalUsage``: Total filesystem usage (as a percentage)
     - ``Periodic(freq)``: Run periodically (e.g., ``"daily"``, ``"hourly"``)
     - ``Scheduled(datetime)``: Run at a specific time (e.g.,
       ``"2025-06-01 03:00"``)

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
- Configuring parameters such as thread count, error suspension, and rate
  limiting.
- Automatically triggering every 10 minutes.
- Ignoring files owned by ``root`` or ``nfsnobody``, as well as files matching
  ``work`` or ``somegroup`` fileclasses.
- Cleaning up files older than 60 days based on last access and creation time.
