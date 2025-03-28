.. This file is part of the RobinHood Library
   Copyright (C) 2025 Commissariat à l'énergie atomique et
                      aux énergies alternatives

   SPDX-License-Identifier: LGPL-3.0-or-later

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
those functions (along with useful constant and function like actions) into the
execution context of the config file.
This way, when the configuration file runs and calls ``declare_policy(...)``,
it is actually calling the engine’s internal function, which stores the policy
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

However, the engine must also perform explicit validation at the time of each
declaration to ensure that the inputs conform to expected types and structures.

Each declaration function (``declare_fileclass``, ``declare_policy``, etc.) is
responsible for validating the consistency and type correctness of its arguments
immediately upon invocation. This includes:

- Ensuring all required parameters are present and of the correct type
- Checking that ``target`` and ``condition`` expressions are well-formed and
  evaluable
- Verifying that the declared ``action`` is callable or a valid external command
- Enforcing that all trigger functions exist and receive compatible arguments
- Ensuring ``policy`` and ``rule`` names are unique within the configuration
  context

These validations prevent the engine from accepting partially valid
configurations and help users catch issues early, before execution begins.

If a validation fails, a descriptive error messages should be raised, clearly
indicating:
- the location of the error (e.g., line number or declaration name),
- the nature of the inconsistency,
- and the expected value or format.

Building Conditions in the Configuration File
---------------------------------------------

In configuration files, conditions are written using standard Python
expressions.

For example:
.. code:: python

    (Type == "dir") & ((Dircount > 500) | (User == "admin"))

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
intended to run. Even in manual execution mode, this evaluation is performed.

The trigger evaluation behaviour differs depending on the execution context:
- ``Manual execution`` (via ``rbh-policy``):
  The engine starts by filtering entries based on the default target condition
  of the policy. The trigger is then evaluated against this filtered set.
  If the trigger is satisfied, the engine proceeds with the evaluation and
  execution of the policy (including rules and actions). If not, the execution
  stops at this stage.

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

  It is important to note that this query reflects the state of the database at
  the time of execution, which may not be fully up to date with the live
  filesystem.

  Then, for each entry in this list, the engine evaluates the rules sequentially,
  in the order they are defined in the policy configuration:

  1. The engine checks if the entry satisfies the condition of the first rule.
     If it matches, the corresponding action is executed, and the engine
     immediately proceeds to the next entry.
  2. If the entry does not match the first rule, the engine evaluates the next
     rule.
  3. This process continues until a rule matches.
  4. If no rule matches, the policy's default action is applied to the entry.

  Note: Rules are evaluated based on the entry’s state in the metadata database.
  However, before applying the default action (when no rule matched), the engine
  performs a final verification against the live filesystem to ensure the entry
  is still valid and consistent with the database. This step prevents executing
  the default action on stale or outdated entries.

  For example, if the policy target is ``Size > 100MB``, and the following
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
- If a rule does not specify an action or parameters, the policy's default
  action and parameters will be applied. In this case, the rule has no effect
  and can be considered redundant.

To explicitly ignore certain entries, a rule can set its ``action`` field to
``None``. This is useful when you want to skip processing for specific subsets
of files without needing to define a separate policy.

6. **Logging Execution Details**

During the execution of a policy, the Policy Engine provides detailed logging to
make its behavior transparent and traceable.

Summary Report (always displayed):
At the end of execution, a summary report is printed with aggregated information
such as:
- Total number of entries processed.
- Number of entries per rule (including those with action ``None``).
- Whether the default rule was used.
- Total number of errors (if any).
- Total execution time.
- Average processing rate (entries per second).

Detailed Logging (Only with ``--verbose`` in Manual Mode):

For each policy run, the following information is logged:
- The ``name of the policy`` currently being executed.
- The ``rules`` being evaluated and their associated conditions.
- For each rule that matches entries, the engine logs:
  - The ``action`` applied.
  - The ``entries`` affected by this rule (this output is configurable via the
    ``--verbose`` option, see :ref:`manual-mode` for more details).

- If a rule does not match any entries, this is also indicated in the logs.
- If an entry does not match any rule and the default action is used, this is
  also explicitly logged.

In the case of any error during execution (e.g., malformed condition, failed
external command, missing parameters), the error is logged with enough detail to
understand.

This logging mechanism ensures administrators can track the policy execution
process step by step, and easily identify configuration issues or unexpected
behaviors.
