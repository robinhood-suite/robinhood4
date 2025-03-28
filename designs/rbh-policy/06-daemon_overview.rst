.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

Daemon Overview
===============

The daemon in RobinHood 4 runs in the background to automatically enforce
policies based on triggers. Each trigger defines how and when to evaluate
conditions, allowing the engine to react promptly without scanning the entire
system or requiring manual intervention.

Launching the Daemon
--------------------

The RobinHood policy daemon is typically run as a long-lived service, with one
instance supervising a specific filesystem and its associated policies.

When managed by ``systemd``, the templated unit ``rbh-policy@<fsname>.service``
uses the filesystem name (e.g., ``fs1``) to locate the appropriate policy
configuration file.

An optional environment file can define which policies are activated for the
instance, for example:

    ``RBH_POLICIES="cleanup,archive"``

This allows administrators to tailor policy execution on a per-filesystem basis.

Trigger Evaluation Loop
-----------------------

Upon starting, the daemon parses the policy configuration file and constructs
an internal dictionary of policy objects. Each policy includes a trigger, which
defines the condition under which the policy should be automatically executed.

To enable efficient monitoring and execution, the daemon classifies triggers into
two main categories based on their nature:

1. Time-based Triggers:

Time-based triggers include ``Scheduled(datetime)`` and ``Periodic(freq)``,
which rely solely on time to determine when a policy should be executed. These do
not depend on any filesystem state or usage metrics.

For efficient handling, the daemon extracts all time-based triggers from the
policies and stores them in a dedicated internal data structure. Each entry in
this structure links the trigger to its associated policy, enabling quick lookup
during execution.

This asynchronous approach allows the daemon to monitor many timers concurrently
within a single event loop, ensuring:
- Efficiency: No additional threads or processes are required.
- Responsiveness: Triggers are executed as soon as conditions are met.
- Scalability: Hundreds of time-based policies can be scheduled without
  affecting performance.

Periodic triggers automatically reschedule themselves after each run, while
scheduled triggers run once at the specified date and time.

2. Metrics-based Triggers:

Metrics-based triggers rely on dynamic system state such as user or group file
counts, disk usage, or total filesystem occupancy. These are evaluated
periodically based on updated system metrics.

To optimize performance, the daemon extracts all non-time-based triggers at
initialization and stores them in a dedicated internal data structure. Each entry
in this structure retains a reference to its originating policy, ensuring that
trigger evaluation and policy dispatch remain tightly coupled.

The required metrics for evaluating these triggers are retrieved from the given
database URI which acts as the central state repository for the system.

These triggers include:
- ``UserFileCount(users)``: File count of one or more users
- ``UserDiskUsage(users)``: Disk usage of one or more users
- ``GroupFileCount(groups)``: File count of one or more groups
- ``GroupDiskUsage(groups)``: Disk usage of one or more groups
- ``GlobalSizePercent``: Total filesystem disk usage (percentage)
- ``GlobalInodePercent``: Total filesystem inode usage (percentage)

Trigger evaluation occurs at a fixed interval, defined by the mandatory
``evaluation_interval`` parameter in the configuration.

To prevent redundant or overlapping policy executions, the daemon maintains
an internal execution state for each policy. When a policy is triggered,
its status is marked as "running". As long as the policy remains in this
state, any subsequent trigger evaluations that would normally re-fire the
same policy are suppressed.

Once the policy execution completes, its state is reset, and it becomes eligible
for triggering again during subsequent evaluations.

Once a policy has been triggered by the daemon, its execution is handled using
the same mechanism as for manual execution.
For a detailed breakdown of how a policy is executed, see section 04:
``Policy Execution Flow`` in the ``implementation_overview.rst``.
