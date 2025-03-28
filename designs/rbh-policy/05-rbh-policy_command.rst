.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

rbh-policy Command
==================

The ``rbh-policy`` command is the main entry point to interact with the
RobinHood 4 Policy Engine. It supports two main execution modes:

- Manual execution of one or more policies (via ``rbh-policy``).
- Daemon mode for automatic and continuous policy monitoring based on triggers.

This command-line interface acts as a unified tool for both administrators and
automation systems to interact with the policy engine.

.. _manual-mode:

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
  - ``uid:<uid>``: Only entries owned by the specified numeric user ID.
  - ``group:<groupname>``: Only entries belonging to the specified group.
  - ``gid:<gid>``: Only entries belonging to the specified numeric group ID.
  - ``file:<path>``: A specific file or directory.
  - ``class:<fileclass>``: Entries matching the given fileclass.
  - ``ost:<ost_idx>``: Entries located on the specified OST index.
  - ``pool:<poolname>``: Entries located in the given OST pool.
  - ``mdt:<mdt_index>``: Entries stored on the specified MDT (Metadata Target).

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

**Verbosity Control**

The verbosity level during policy execution can be configured entirely from the
command line, with no need to modify the configuration file. Three levels of
control are supported:

1. ``Global Verbosity (via --verbose)``: Enables detailed logging for all
   policies and rules.

2. ``Per-Policy Verbosity (via verbose= parameter):``
   The ``verbose`` parameter can be specified per policy invocation to override
   the global behavior.

   - ``verbose=true``: enables full verbosity for that policy.
   - ``verbose=false`` (or omitted): disables detailed logging for that policy.
   - ``verbose=rule1,rule2,...``: restricts verbosity to the named rules only
     (default for the default policy rule).

   .. code:: bash

      rbh-policy fs1 cleanup(verbose=true),archive(verbose=false)

      # Verbose only for selected rules:
      rbh-policy fs1 cleanup(verbose=old_files,huge_files)

3. ``Precedence``:
   - If ``--verbose`` is set globally, it enables verbosity for all policies
     unless overridden.
   - The ``verbose=`` parameter takes precedence over the global flag for each
     policy.

This allows for flexible debugging and inspection of behavior, for example:

.. code:: bash

   # Run all policies with default logging
   rbh-policy fs1 all

   # Enable verbosity only for the 'cleanup' policy
   rbh-policy fs1 cleanup(verbose=true),archive

   # Verbose logging only for specific rules within 'cleanup'
   rbh-policy fs1 cleanup(verbose=rule1,rule2),archive

Future improvement (verbosity output separation):
It may be useful to support logging matched entries to separate files per policy
or even per rule when using ``--verbose``. This would improve traceability and
post-processing of dry-run results.

For example:

- ``/var/log/robinhood4/fs1/policy-cleanup.log``
- ``/var/log/robinhood4/fs1/policy-cleanup-rule-old_files.log``

**Examples:**

.. code:: bash

   # Run the archive policy on all entries in pool0, up to 1TB
   rbh-policy fs1 archive(target=pool:pool0,max-vol=1TB)

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

   # Start in background (detach mode)
   rbh-policy --daemon fs1 cleanup --detach

Arguments:
- ``--daemon``: Enables daemon mode.
- ``<policy1>,<policy2>,...``: One or more policy names declared in the config.
- ``all``: Special keyword to monitor all policies defined in the file.

Available options:
- ``--config <file.py>``: An additional configuration file.
- ``--verbose``: Enables detailed output during daemon execution, including
  matched entries, triggered rules, and executed actions.
- ``--detach``: Runs the daemon in background (non-blocking mode, only outside
                of systemd).

**Detach Mode**

When the daemon is launched manually (i.e., outside of systemd), it runs in the
foreground by default. To run it in background mode, you can use the ``--detach``
option.

When ``--detach`` is specified:
- The process forks into the background after initialization.
- Standard output is redirected (e.g., to syslog or log file).
- Startup errors (e.g., bad configuration) are still reported before detaching.

This is useful for manual deployments, cron jobs, or testing environments where
systemd is not used.

Exit Codes
----------

- ``0``: Success
- ``1``: Invalid configuration or arguments
- ``2``: Runtime or execution error
