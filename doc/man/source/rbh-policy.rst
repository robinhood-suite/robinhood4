rbh-policy(1)
=============

SYNOPSIS
--------

**rbh-policy** **run** *FS_NAME* *POLICY1[,POLICY2,...]*

DESCRIPTION
-----------

Execute one or more RobinHood4 Policy Engine policies.

The command loads a Python policy configuration, selects the requested
policy names, then evaluates each policy target and applies its action on
matching entries.

Policies are executed in the exact order they are provided on the command
line.

ARGUMENTS
---------

**FS_NAME**
    Configuration selector used to locate the policy configuration file.

    If *FS_NAME* does not contain ``/``, the file loaded is:
    ``/etc/robinhood4.d/<FS_NAME>.py``.

    Relative paths containing ``/`` are rejected.

    If *FS_NAME* is an absolute path, that path is loaded directly.
    The file must be a Python configuration file (typically with ``.py``).


**POLICY1[,POLICY2,...]**
    Comma-separated list of policy names to execute.

    The list is split strictly on commas. Do not use spaces in the list.

EXAMPLES
--------

Run a single policy from ``/etc/robinhood4.d/fs1.py``
    $ rbh-policy run fs1 cleanup

Run multiple policies from ``/etc/robinhood4.d/fs1.py``
    $ rbh-policy run fs1 cleanup,archive,log_only

Run a policy from an explicit configuration file
    $ rbh-policy run /tmp/fs1.py cleanup

NOTES
-----

A practical workflow is:

1. Synchronize metadata first (for example with rbh-sync(1) and
   rbh-fsevents(1)).
2. Validate policies with safe actions such as logging.
3. Execute destructive policies only after validation.

SEE ALSO
--------

rbh-sync(1), rbh-fsevents(1), rbh-policy-conf(5), robinhood4(7)

CREDITS
-------

rbh-policy and RobinHood4 is written by the storage software development team
of the CEA (Commissariat a l'energie atomique et aux energies alternatives).

It can be contacted at st-hpc@saxifrage.saclay.cea.fr.
